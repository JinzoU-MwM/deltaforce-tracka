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
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\zoom.txt",L"w"); if(!f) return 1;
    uintptr_t w=D(R<uint64_t>(B+0x1D9E4E78));
    uintptr_t gi=D(R<uint64_t>(w+0x190));
    uintptr_t lpA=D(R<uint64_t>(gi+0x38)), lp=D(R<uint64_t>(lpA)), pc=D(R<uint64_t>(lp+0x30));
    uintptr_t pawn=D(R<uint64_t>(pc+0x3F0));
    uintptr_t bb=D(R<uint64_t>(pawn+0xE70));
    fprintf(f,"pawn=%p bb=%p zoom515=%d fire516=%d weapon4A8=%p\n",(void*)pawn,(void*)bb,R<uint8_t>(bb+0x515),R<uint8_t>(bb+0x516),(void*)D(R<uint64_t>(bb+0x4A8)));
    uintptr_t wp=D(R<uint64_t>(bb+0x4A8));
    fprintf(f,"weaponID828=%llu\n",(unsigned long long)R<uint64_t>(wp+0x828));
    uintptr_t cm=D(R<uint64_t>(pc+0x408));
    fprintf(f,"CM=%p floats 40..130 in 0x300..0x700:\n",(void*)cm);
    for(DWORD o=0x300;o<0x700;o+=4){ float v=R<float>(cm+o); if(v>=40.f&&v<=130.f) fprintf(f,"  @%03X = %.2f\n",o,v); }
    uintptr_t lpobj=D(R<uint64_t>(pc+0x3E8));
    fprintf(f,"LP=%p floats 40..130 in 0..0x400:\n",(void*)lpobj);
    for(DWORD o=0;o<0x400;o+=4){ float v=R<float>(lpobj+o); if(v>=40.f&&v<=130.f) fprintf(f,"  @%03X = %.2f\n",o,v); }
    fprintf(f,"DONE\n"); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
