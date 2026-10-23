#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
static bool Readable(uintptr_t a,size_t n){ if(!a)return false; MEMORY_BASIC_INFORMATION m{}; if(!VirtualQuery((LPCVOID)a,&m,sizeof(m)))return false; return m.State==MEM_COMMIT&&!(m.Protect&PAGE_NOACCESS)&&(uintptr_t)m.BaseAddress+m.RegionSize>=a+n; }
static DWORD WINAPI Th(LPVOID){
    uintptr_t B=(uintptr_t)GetModuleHandleW(nullptr);
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\calls.txt",L"w"); if(!f) return 1;
    uintptr_t sites[12]={0x1412C5B7CULL,0x14145813AULL,0x1414581AFULL,0x14145821CULL,0x141458289ULL,0x141BE6F3EULL,0x141BE6FD1ULL,0x141BE70A5ULL,0x141DC80AEULL,0x141E31C85ULL,0x14222BCA7ULL,0x142231658ULL};
    for(int k=0;k<12;k++){
        uintptr_t a=sites[k]-0x300;
        fprintf(f,"SITE %p START %p\n",(void*)sites[k],(void*)a);
        if(!Readable(a,0x400)){ fprintf(f,"unreadable\n"); continue; }
        const unsigned char* p=(const unsigned char*)a;
        for(int i=0;i<0x400;i++){ fprintf(f,"%02X",p[i]); if((i&31)==31) fprintf(f,"\n"); }
    }
    fprintf(f,"DONE\n"); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
