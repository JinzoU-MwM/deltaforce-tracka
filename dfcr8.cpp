// dfcr8 — injector alternatif: VirtualAllocEx + WriteProcessMemory + CreateRemoteThread
// Dipakai saat NtMapViewOfSection diblokir ACE (0xC0000022)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <vector>

static std::vector<BYTE> ReadAll(const wchar_t* p){
    HANDLE f=CreateFileW(p,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(f==INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER sz{}; GetFileSizeEx(f,&sz);
    std::vector<BYTE> b((size_t)sz.QuadPart);
    DWORD rd=0; ReadFile(f,b.data(),(DWORD)b.size(),&rd,nullptr);
    b.resize(rd); CloseHandle(f); return b;
}
static void Log(const char* fmt, ...){
    va_list ap; va_start(ap,fmt); vprintf(fmt,ap); va_end(ap); printf("\n"); fflush(stdout);
}

int wmain(int argc, wchar_t** argv){
    if(argc<3){ printf("usage: dfcr8 <pid> <dll>\n"); return 1; }
    DWORD pid=(DWORD)_wtoi(argv[1]); const wchar_t* dll=argv[2];
    std::vector<BYTE> file=ReadAll(dll);
    if(file.size()<0x1000){ Log("read dll failed"); return 2; }
    auto dos=(PIMAGE_DOS_HEADER)file.data();
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE){ Log("bad dos"); return 2; }
    auto nt=(PIMAGE_NT_HEADERS)(file.data()+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE){ Log("bad nt"); return 2; }
    DWORD imgSize=nt->OptionalHeader.SizeOfImage;
    std::vector<BYTE> img(imgSize,0);
    memcpy(img.data(),file.data(),min((size_t)nt->OptionalHeader.SizeOfHeaders,file.size()));
    auto sec=IMAGE_FIRST_SECTION(nt);
    for(WORD i=0;i<nt->FileHeader.NumberOfSections;++i,++sec){
        if(!sec->SizeOfRawData) continue;
        DWORD n=min((size_t)sec->SizeOfRawData,(size_t)sec->Misc.VirtualSize?sec->Misc.VirtualSize:sec->SizeOfRawData);
        if(sec->VirtualAddress+n>imgSize) n=imgSize-sec->VirtualAddress;
        memcpy(img.data()+sec->VirtualAddress,file.data()+sec->PointerToRawData,n);
    }
    HANDLE h=OpenProcess(PROCESS_ALL_ACCESS,FALSE,pid);
    Log("open pid=%lu h=%p err=%lu",(unsigned long)pid,h,h?0:GetLastError());
    if(!h) return 3;

    // alloc RWX di target
    void* remote=VirtualAllocEx(h,nullptr,imgSize,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);
    Log("VirtualAllocEx remote=%p err=%lu",remote,remote?0:GetLastError());
    if(!remote){ CloseHandle(h); return 4; }

    // tulis image
    if(!WriteProcessMemory(h,remote,img.data(),imgSize,nullptr)){
        Log("WriteProcessMemory failed err=%lu",GetLastError());
        VirtualFreeEx(h,remote,0,MEM_RELEASE); CloseHandle(h); return 5;
    }
    Log("WriteProcessMemory ok (%lu bytes)",(unsigned long)imgSize);

    // relokasi
    uintptr_t delta=(uintptr_t)remote-nt->OptionalHeader.ImageBase;
    auto& rd=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if(delta&&rd.Size){
        auto blk=(PIMAGE_BASE_RELOCATION)(img.data()+rd.VirtualAddress);
        auto end=blk; auto endb=(BYTE*)(img.data()+rd.VirtualAddress+rd.Size);
        int n=0;
        while((BYTE*)blk<endb&&blk->SizeOfBlock){
            DWORD cnt=(blk->SizeOfBlock-sizeof(IMAGE_BASE_RELOCATION))/sizeof(WORD);
            auto ent=(PWORD)((BYTE*)blk+sizeof(IMAGE_BASE_RELOCATION));
            for(DWORD i=0;i<cnt;i++){
                WORD t=ent[i]>>12; WORD off=ent[i]&0xFFF;
                auto patch=(uint64_t*)(img.data()+blk->VirtualAddress+off);
                if(t==IMAGE_REL_BASED_DIR64) *patch+=(uint64_t)delta;
                else if(t==IMAGE_REL_BASED_HIGHLOW) *(uint32_t*)patch+=(uint32_t)delta;
            }
            blk=(PIMAGE_BASE_RELOCATION)((BYTE*)blk+blk->SizeOfBlock); n++;
        }
        Log("reloc blocks=%d delta=%llX",n,(unsigned long long)delta);
    }

    // resolve import
    auto& idir=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    int ni=0;
    if(idir.Size){
        auto imp=(PIMAGE_IMPORT_DESCRIPTOR)(img.data()+idir.VirtualAddress);
        for(;imp->Name;imp++){
            const char* dllname=(const char*)img.data()+imp->Name;
            HMODULE hm=LoadLibraryA(dllname);
            if(!hm){ Log("LoadLibrary %s failed",dllname); continue; }
            auto thunk=(PIMAGE_THUNK_DATA)(img.data()+imp->OriginalFirstThunk?imp->OriginalFirstThunk:imp->FirstThunk);
            auto ft=(PIMAGE_THUNK_DATA)(img.data()+imp->FirstThunk);
            for(;thunk->u1.AddressOfData;thunk++,ft++){
                const char* fn;
                if(IMAGE_SNAP_BY_ORDINAL(thunk->u1.Ordinal)) fn=(const char*)(uintptr_t)IMAGE_ORDINAL(thunk->u1.Ordinal);
                else fn=((PIMAGE_IMPORT_BY_NAME)(img.data()+thunk->u1.AddressOfData))->Name;
                FARPROC p;
                if(IMAGE_SNAP_BY_ORDINAL(thunk->u1.Ordinal)) p=GetProcAddress(hm,(LPCSTR)IMAGE_ORDINAL(thunk->u1.Ordinal));
                else p=GetProcAddress(hm,fn);
                if(p) ft->u1.Function=(uint64_t)p;
            }
            ni++;
        }
        Log("imports resolved for %d dlls",ni);
    }

    // tulis ulang setelah reloc+import
    if(!WriteProcessMemory(h,remote,img.data(),imgSize,nullptr)){
        Log("rewrite failed err=%lu",GetLastError()); return 6;
    }

    // panggil DllMain lewat CreateRemoteThread: LoadLibrary-alike
    // Trik: pakai entry point DLL langsung (DllMain) dengan param faked
    DWORD entryRva=nt->OptionalHeader.AddressOfEntryPoint;
    if(!entryRva){ Log("no entry point"); return 7; }
    void* remoteEntry=(BYTE*)remote+entryRva;

    // CreateRemoteThread(entry, remote, DLL_PROCESS_ATTACH=1, 0)
    HANDLE th=CreateRemoteThread(h,nullptr,0,(LPTHREAD_START_ROUTINE)remoteEntry,remote,0,nullptr);
    Log("CreateRemoteThread entry=%p th=%p err=%lu",remoteEntry,th,th?0:GetLastError());
    if(!th){
        Log("CRT failed err=%lu",GetLastError());
        VirtualFreeEx(h,remote,0,MEM_RELEASE); CloseHandle(h); return 8;
    }
    WaitForSingleObject(th,8000);
    DWORD ec=0; GetExitCodeThread(th,&ec);
    Log("thread exit=%lu",ec);
    CloseHandle(th); CloseHandle(h);
    Log("DONE injected");
    return 0;
}
