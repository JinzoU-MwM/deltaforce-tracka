#pragma once
#include <windows.h>
#include <vector>

inline volatile long g_mapStep = 0;

static bool ManualMap(HANDLE hProc, const wchar_t* dllPath, DWORD* err) {
    *err = 0;
    g_mapStep = 1;
    HANDLE f = CreateFileW(dllPath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) { *err = GetLastError(); return false; }
    DWORD sz = GetFileSize(f, nullptr);
    std::vector<BYTE> buf(sz);
    DWORD rd = 0;
    ReadFile(f, buf.data(), sz, &rd, nullptr);
    CloseHandle(f);
    if (rd != sz || sz < 0x1000) { *err = 1; return false; }

    g_mapStep = 2;
    auto dos = (PIMAGE_DOS_HEADER)buf.data();
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) { *err = 2; return false; }
    auto nt = (PIMAGE_NT_HEADERS64)(buf.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) { *err = 3; return false; }

    SIZE_T imgSize = nt->OptionalHeader.SizeOfImage;
    g_mapStep = 3;
    BYTE* base = (BYTE*)VirtualAllocEx(hProc, nullptr, imgSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!base) { *err = GetLastError(); return false; }

    g_mapStep = 4;
    if (!WriteProcessMemory(hProc, base, buf.data(), nt->OptionalHeader.SizeOfHeaders, nullptr)) { *err = GetLastError(); return false; }
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (!sec->SizeOfRawData) continue;
        WriteProcessMemory(hProc, base + sec->VirtualAddress, buf.data() + sec->PointerToRawData, sec->SizeOfRawData, nullptr);
    }

    g_mapStep = 5;
    auto At = [&](DWORD rva) -> BYTE* {
        auto s2 = IMAGE_FIRST_SECTION(nt);
        for (WORD i2 = 0; i2 < nt->FileHeader.NumberOfSections; ++i2, ++s2) {
            DWORD vs = s2->Misc.VirtualSize ? s2->Misc.VirtualSize : s2->SizeOfRawData;
            if (rva >= s2->VirtualAddress && rva < s2->VirtualAddress + vs) {
                DWORD off = rva - s2->VirtualAddress;
                if (off >= s2->SizeOfRawData) return nullptr;
                return buf.data() + s2->PointerToRawData + off;
            }
        }
        return nullptr;
    };

    uintptr_t delta = (uintptr_t)base - nt->OptionalHeader.ImageBase;
    auto& relDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (delta && relDir.Size) {
        BYTE* rp = At(relDir.VirtualAddress);
        BYTE* rend = rp ? rp + relDir.Size : nullptr;
        while (rp && rend && rp + sizeof(IMAGE_BASE_RELOCATION) <= rend) {
            auto blk = (PIMAGE_BASE_RELOCATION)rp;
            if (!blk->SizeOfBlock || blk->SizeOfBlock < sizeof(IMAGE_BASE_RELOCATION)) break;
            DWORD n = (blk->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            WORD* e = (WORD*)(blk + 1);
            for (DWORD i = 0; i < n; ++i) {
                int type = e[i] >> 12, off = e[i] & 0xFFF;
                BYTE* p = base + blk->VirtualAddress + off;
                if (type == IMAGE_REL_BASED_DIR64) {
                    uint64_t v = 0;
                    if (ReadProcessMemory(hProc, p, &v, 8, nullptr)) { v += delta; WriteProcessMemory(hProc, p, &v, 8, nullptr); }
                } else if (type == IMAGE_REL_BASED_HIGHLOW) {
                    uint32_t v = 0;
                    if (ReadProcessMemory(hProc, p, &v, 4, nullptr)) { v += (uint32_t)delta; WriteProcessMemory(hProc, p, &v, 4, nullptr); }
                }
            }
            rp += blk->SizeOfBlock;
        }
    }

    g_mapStep = 6;
    auto& impDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (impDir.Size) {
        BYTE* dp = At(impDir.VirtualAddress);
        BYTE* dend = dp ? dp + impDir.Size : nullptr;
        while (dp && dend && dp + sizeof(IMAGE_IMPORT_DESCRIPTOR) <= dend) {
            auto desc = (PIMAGE_IMPORT_DESCRIPTOR)dp;
            if (!desc->Name) break;
            const char* dllName = (const char*)At(desc->Name);
            if (!dllName) { *err = 4; return false; }
            HMODULE mod = GetModuleHandleA(dllName);
            if (!mod) mod = LoadLibraryA(dllName);
            if (!mod) { *err = 4; return false; }
            BYTE* tp = At(desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk);
            DWORD idx = 0;
            while (tp) {
                auto thunk = (PIMAGE_THUNK_DATA64)(tp + (SIZE_T)idx * 8);
                if (!thunk->u1.AddressOfData) break;
                uintptr_t fn = 0;
                if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)
                    fn = (uintptr_t)GetProcAddress(mod, (LPCSTR)(thunk->u1.Ordinal & 0xFFFF));
                else {
                    auto ibn = (PIMAGE_IMPORT_BY_NAME)At((DWORD)thunk->u1.AddressOfData);
                    if (!ibn) { *err = 5; return false; }
                    fn = (uintptr_t)GetProcAddress(mod, (LPCSTR)ibn->Name);
                }
                if (!fn) { *err = 5; return false; }
                WriteProcessMemory(hProc, base + desc->FirstThunk + (SIZE_T)idx * 8, &fn, 8, nullptr);
                ++idx;
            }
            dp += sizeof(IMAGE_IMPORT_DESCRIPTOR);
        }
    }

    g_mapStep = 7;
    sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        DWORD ch = sec->Characteristics, prot = PAGE_READONLY;
        if (ch & IMAGE_SCN_MEM_EXECUTE) prot = (ch & IMAGE_SCN_MEM_WRITE) ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
        else if (ch & IMAGE_SCN_MEM_WRITE) prot = PAGE_READWRITE;
        DWORD old = 0;
        VirtualProtectEx(hProc, base + sec->VirtualAddress, sec->Misc.VirtualSize ? sec->Misc.VirtualSize : sec->SizeOfRawData, prot, &old);
    }

    uintptr_t rtlAdd = (uintptr_t)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlAddFunctionTable");
    auto& excDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    uintptr_t tableAddr = 0; uint32_t tableCount = 0;
    if (excDir.Size >= 12) { tableAddr = (uintptr_t)base + excDir.VirtualAddress; tableCount = excDir.Size / 12; }

    BYTE stub[128]; int k = 0;
    if (tableAddr && rtlAdd) {
        stub[k++] = 0x48; stub[k++] = 0xB9; memcpy(stub + k, &tableAddr, 8); k += 8;   // mov rcx, table
        stub[k++] = 0xBA; memcpy(stub + k, &tableCount, 4); k += 4;                     // mov edx, count
        stub[k++] = 0x49; stub[k++] = 0xB8; { uintptr_t b = (uintptr_t)base; memcpy(stub + k, &b, 8); } k += 8; // mov r8, base
        stub[k++] = 0x48; stub[k++] = 0xB8; memcpy(stub + k, &rtlAdd, 8); k += 8;       // mov rax, rtlAdd
        stub[k++] = 0x48; stub[k++] = 0x83; stub[k++] = 0xEC; stub[k++] = 0x28;         // sub rsp,28
        stub[k++] = 0xFF; stub[k++] = 0xD0;                                             // call rax
        stub[k++] = 0x48; stub[k++] = 0x83; stub[k++] = 0xC4; stub[k++] = 0x28;         // add rsp,28
    }
    stub[k++] = 0x48; stub[k++] = 0xB9; memcpy(stub + k, &base, 8); k += 8;
    stub[k++] = 0xBA; uint32_t one = 1; memcpy(stub + k, &one, 4); k += 4;
    stub[k++] = 0x45; stub[k++] = 0x31; stub[k++] = 0xC0;
    stub[k++] = 0x48; stub[k++] = 0xB8; uintptr_t entry = (uintptr_t)base + nt->OptionalHeader.AddressOfEntryPoint; memcpy(stub + k, &entry, 8); k += 8;
    stub[k++] = 0x48; stub[k++] = 0x83; stub[k++] = 0xEC; stub[k++] = 0x28;
    stub[k++] = 0xFF; stub[k++] = 0xD0;
    stub[k++] = 0x48; stub[k++] = 0x83; stub[k++] = 0xC4; stub[k++] = 0x28;
    stub[k++] = 0xC3;

    g_mapStep = 8;
    BYTE* remoteStub = (BYTE*)VirtualAllocEx(hProc, nullptr, sizeof(stub), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteStub) { *err = GetLastError(); return false; }
    WriteProcessMemory(hProc, remoteStub, stub, k, nullptr);
    DWORD old = 0;
    VirtualProtectEx(hProc, remoteStub, sizeof(stub), PAGE_EXECUTE_READ, &old);

    g_mapStep = 9;
    HANDLE t = CreateRemoteThread(hProc, nullptr, 0, (LPTHREAD_START_ROUTINE)remoteStub, nullptr, 0, nullptr);
    if (!t) { *err = GetLastError(); return false; }
    WaitForSingleObject(t, 15000);
    DWORD ec = 0;
    GetExitCodeThread(t, &ec);
    CloseHandle(t);
    VirtualFreeEx(hProc, remoteStub, 0, MEM_RELEASE);

    if (ec == STILL_ACTIVE) { *err = 6; return false; }

    g_mapStep = 10;
    BYTE zeros[0x1000] = {};
    if (VirtualProtectEx(hProc, base, sizeof(zeros), PAGE_READWRITE, &old))
        WriteProcessMemory(hProc, base, zeros, sizeof(zeros), nullptr);
    return true;
}
