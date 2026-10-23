#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
static bool Readable(uintptr_t a,size_t n){ if(!a)return false; MEMORY_BASIC_INFORMATION m{}; if(!VirtualQuery((LPCVOID)a,&m,sizeof(m)))return false; return m.State==MEM_COMMIT&&!(m.Protect&PAGE_NOACCESS)&&(uintptr_t)m.BaseAddress+m.RegionSize>=a+n; }
template<class T> static T R(uintptr_t a){ T v{}; if(Readable(a,sizeof(T))) memcpy(&v,(void*)a,sizeof(T)); return v; }
static uintptr_t D(uint64_t v){ return (uintptr_t)(v & 0xFFFFFFFFFFFFULL); }
static DWORD WINAPI Th(LPVOID){
    uintptr_t B=(uintptr_t)GetModuleHandleW(nullptr);
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\vt.txt",L"w"); if(!f) return 1;
    uintptr_t w=D(R<uint64_t>(B+0x1D9E4E78));
    uintptr_t gi=D(R<uint64_t>(w+0x190));
    uintptr_t lpA=D(R<uint64_t>(gi+0x38)), lp=D(R<uint64_t>(lpA)), pc=D(R<uint64_t>(lp+0x30));
    uintptr_t vt=D(R<uint64_t>(pc));
    fprintf(f,"PC=%p VT=%p\n",(void*)pc,(void*)vt);
    for(int i=0x38;i<=0x58;i++){
        uintptr_t fn=D(R<uint64_t>(vt+(uint64_t)i*8));
        if(fn<B||fn>=B+0x20000000ULL||!Readable(fn,0x200)) continue;
        fprintf(f,"IDX %02X FN %p\n",i,(void*)fn);
        const unsigned char* p=(const unsigned char*)fn;
        for(int o=0;o<0x200;o++){ fprintf(f,"%02X",p[o]); if((o&31)==31) fprintf(f,"\n"); }
    }
    fprintf(f,"DONE\n"); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
