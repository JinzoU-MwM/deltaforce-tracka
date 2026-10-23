#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

static bool Readable(uintptr_t a, size_t n) {
    if (!a) return false;
    MEMORY_BASIC_INFORMATION m{};
    if (!VirtualQuery((LPCVOID)a, &m, sizeof(m))) return false;
    return m.State == MEM_COMMIT && !(m.Protect & PAGE_NOACCESS) && (uintptr_t)m.BaseAddress + m.RegionSize >= a + n;
}
static DWORD WINAPI Th(LPVOID) {
    uintptr_t B = (uintptr_t)GetModuleHandleW(nullptr);
    FILE* f = nullptr; _wfopen_s(&f, L"C:\\Users\\User\\trackA\\scan_rt.txt", L"w");
    if (!f) return 1;
    fprintf(f, "BASE=%p\n", (void*)B);
    auto dos = (PIMAGE_DOS_HEADER)B;
    auto nt = (PIMAGE_NT_HEADERS)(B + dos->e_lfanew);
    auto sec = IMAGE_FIRST_SECTION(nt);
    int hits = 0;
    for (WORD si = 0; si < nt->FileHeader.NumberOfSections && hits < 3000; ++si, ++sec) {
        if (!(sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        uintptr_t va = B + sec->VirtualAddress;
        size_t sz = sec->Misc.VirtualSize ? sec->Misc.VirtualSize : sec->SizeOfRawData;
        fprintf(f, "SEC %p size=%llx\n", (void*)va, (unsigned long long)sz);
        if (!Readable(va, sz)) continue;
        const BYTE* p = (const BYTE*)va;
        for (size_t o = 0; o + 10 < sz; ++o) {
            if (p[o] != 0x66) continue;
            size_t i = o + 1; bool rex = false;
            if (p[i] == 0x41 || p[i] == 0x49) { rex = true; ++i; }
            BYTE op = p[i];
            if (op != 0x81 && op != 0x83) continue;
            BYTE modrm = p[i + 1];
            if (((modrm >> 3) & 7) != 7) continue;
            int mod = modrm >> 6, rm = modrm & 7;
            if (mod == 3) continue;
            if (mod == 0 && rm == 5) continue; // RIP-relative
            size_t ds = (mod == 1) ? 1 : (mod == 2) ? 4 : 0;
            size_t imm = i + 2 + ds;
            int32_t disp = 0;
            if (ds == 1) disp = (int8_t)p[i + 2];
            else if (ds == 4) disp = *(int32_t*)(p + i + 2);
            bool ok = false;
            if (op == 0x81) ok = (p[imm] == 0xFF && p[imm+1] == 0xFF);
            else ok = (p[imm] == 0xFF);
            if (!ok) continue;
            fprintf(f, "HIT %p disp=0x%X op=%02X rex=%d\n", (void*)(va + o), disp, op, rex ? 1 : 0);
            size_t st = (o > 48) ? o - 48 : 0;
            for (size_t k = st; k < o + 112 && k < sz; ++k) fprintf(f, "%02X", p[k]);
            fprintf(f, "\n");
            if (++hits >= 3000) break;
        }
    }
    fprintf(f, "HITS=%d\n", hits);
    fclose(f);
    return 0;
}
BOOL APIENTRY DllMain(HMODULE, DWORD r, LPVOID) { if (r == DLL_PROCESS_ATTACH) CreateThread(nullptr, 0, Th, nullptr, 0, nullptr); return TRUE; }
