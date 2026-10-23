#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
static bool Readable(uintptr_t a,size_t n){ if(!a)return false; MEMORY_BASIC_INFORMATION m{}; if(!VirtualQuery((LPCVOID)a,&m,sizeof(m)))return false; return m.State==MEM_COMMIT&&!(m.Protect&PAGE_NOACCESS)&&(uintptr_t)m.BaseAddress+m.RegionSize>=a+n; }
static DWORD WINAPI Th(LPVOID){
    uintptr_t B=(uintptr_t)GetModuleHandleW(nullptr);
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\dump3.bin",L"wb"); if(!f)return 1;
    char h[128]; wsprintfA(h,"B=%p\n",(void*)B); fwrite(h,1,lstrlenA(h),f);
    auto blob=[&](const char* tag,uintptr_t a,DWORD n){ char hh[96]; wsprintfA(hh,"%s %p %lu\n",tag,(void*)a,n); fwrite(hh,1,lstrlenA(hh),f); if(a&&Readable(a,n)) fwrite((void*)a,1,n,f); else {char z=0;fwrite(&z,1,1,f);} };
    blob("RESOLVE",0x142BBD4C0ULL,0x600);
    fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
