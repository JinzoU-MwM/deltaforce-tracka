#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
static bool Readable(uintptr_t a,size_t n){ if(!a)return false; MEMORY_BASIC_INFORMATION m{}; if(!VirtualQuery((LPCVOID)a,&m,sizeof(m)))return false; return m.State==MEM_COMMIT&&!(m.Protect&PAGE_NOACCESS)&&(uintptr_t)m.BaseAddress+m.RegionSize>=a+n; }
template<class T> static T R(uintptr_t a){ T v{}; if(Readable(a,sizeof(T))) memcpy(&v,(void*)a,sizeof(T)); return v; }
static uintptr_t D(uint64_t v){ return (uintptr_t)(v & 0xFFFFFFFFFFFFULL); }
static DWORD WINAPI Th(LPVOID){
    uintptr_t B=(uintptr_t)GetModuleHandleW(nullptr);
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\ctrl2.txt",L"w"); if(!f) return 1;
    uintptr_t w=D(R<uint64_t>(B+0x1D9E4E78));
    uintptr_t gi=D(R<uint64_t>(w+0x190));
    uintptr_t lpA=D(R<uint64_t>(gi+0x38)), lp=D(R<uint64_t>(lpA)), pc=D(R<uint64_t>(lp+0x30));
    fprintf(f,"PC=%p\n",(void*)pc);
    for(DWORD o=0x2C0;o<0x600;o+=4){
        float p=R<float>(pc+o),y=R<float>(pc+o+4),r=R<float>(pc+o+8);
        if(!(p==p&&y==y&&r==r)) continue;
        if(fabsf(p)>89.f||fabsf(y)>360.f||fabsf(r)>5.f) continue;
        if(fabsf(p)<0.01f&&fabsf(y)<0.01f&&fabsf(r)<0.01f) continue;
        const char* mark="";
        float dy=fabsf(y-161.5f); if(dy>180) dy=360-dy;
        if(dy<15.f) mark="  <== YAW~161";
        fprintf(f,"+%03X rot=(%.3f,%.3f,%.3f)%s\n",o,p,y,r,mark);
    }
    fprintf(f,"DONE\n"); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
