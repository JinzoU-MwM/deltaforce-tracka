#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
static bool Readable(uintptr_t a,size_t n){ if(!a)return false; MEMORY_BASIC_INFORMATION m{}; if(!VirtualQuery((LPCVOID)a,&m,sizeof(m)))return false; return m.State==MEM_COMMIT&&!(m.Protect&PAGE_NOACCESS)&&(uintptr_t)m.BaseAddress+m.RegionSize>=a+n; }
static DWORD WINAPI Th(LPVOID){
    uintptr_t B=(uintptr_t)GetModuleHandleW(nullptr);
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\funcb.txt",L"w"); if(!f) return 1;
    uintptr_t th[2]={0x153499960ULL,0x1509B9420ULL};
    for(int k=0;k<2;k++){
        fprintf(f,"THUNK %p\n",(void*)th[k]);
        if(!Readable(th[k],256)){ fprintf(f,"unreadable\n"); continue; }
        const unsigned char* p=(const unsigned char*)th[k];
        for(int i=0;i<256;i++){ fprintf(f,"%02X",p[i]); if((i&31)==31) fprintf(f,"\n"); }
    }
    fprintf(f,"BASE=%p\nDONE\n",(void*)B); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
