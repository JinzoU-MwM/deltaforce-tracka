#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
static bool Readable(uintptr_t a,size_t n){ if(!a)return false; MEMORY_BASIC_INFORMATION m{}; if(!VirtualQuery((LPCVOID)a,&m,sizeof(m)))return false; return m.State==MEM_COMMIT&&!(m.Protect&PAGE_NOACCESS)&&(uintptr_t)m.BaseAddress+m.RegionSize>=a+n; }
static DWORD WINAPI Th(LPVOID){
    uintptr_t B=(uintptr_t)GetModuleHandleW(nullptr);
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\scan3.txt",L"w"); if(!f)return 1;
    fprintf(f,"BASE=%p\n",(void*)B);
    auto dos=(PIMAGE_DOS_HEADER)B; auto nt=(PIMAGE_NT_HEADERS)(B+dos->e_lfanew); auto sec=IMAGE_FIRST_SECTION(nt);
    const BYTE pat[]={0x48,0x8D,0x81,0x10,0x02,0x00,0x00};
    int hits=0;
    for(WORD si=0; si<nt->FileHeader.NumberOfSections && hits<200; ++si,++sec){
        if(!(sec->Characteristics&IMAGE_SCN_MEM_EXECUTE)) continue;
        uintptr_t va=B+sec->VirtualAddress; size_t sz=sec->Misc.VirtualSize?sec->Misc.VirtualSize:sec->SizeOfRawData;
        if(!Readable(va,sz)) continue;
        const BYTE* p=(const BYTE*)va;
        for(size_t o=0;o+sizeof(pat)<sz;++o){
            if(memcmp(p+o,pat,sizeof(pat))!=0) continue;
            fprintf(f,"HIT %p\n",(void*)(va+o));
            for(size_t k=(o>32?o-32:0);k<o+96&&k<sz;++k) fprintf(f,"%02X",p[k]);
            fprintf(f,"\n");
            if(++hits>=200) break;
        }
    }
    fprintf(f,"HITS=%d\n",hits); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
