#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
static bool Readable(uintptr_t a,size_t n){ if(!a)return false; MEMORY_BASIC_INFORMATION m{}; if(!VirtualQuery((LPCVOID)a,&m,sizeof(m)))return false; return m.State==MEM_COMMIT&&!(m.Protect&PAGE_NOACCESS)&&(uintptr_t)m.BaseAddress+m.RegionSize>=a+n; }
template<class T> static T R(uintptr_t a){ T v{}; if(Readable(a,sizeof(T))) memcpy(&v,(void*)a,sizeof(T)); return v; }
static uintptr_t D(uint64_t v){ return (uintptr_t)(v & 0xFFFFFFFFFFFFULL); }
static DWORD WINAPI Th(LPVOID){
    uintptr_t B=(uintptr_t)GetModuleHandleW(nullptr);
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\pe.txt",L"w"); if(!f) return 1;
    uintptr_t w=D(R<uint64_t>(B+0x1D9E4E78));
    uintptr_t gi=D(R<uint64_t>(w+0x190));
    uintptr_t lpA=D(R<uint64_t>(gi+0x38)), lp=D(R<uint64_t>(lpA)), pc=D(R<uint64_t>(lp+0x30));
    fprintf(f,"W=%p PC=%p\n",(void*)w,(void*)pc);
    uintptr_t vt=D(R<uint64_t>(pc));
    fprintf(f,"VT=%p\n",(void*)vt);
    int found=0;
    for(int i=0;i<0x200;i++){
        uintptr_t fn=D(R<uint64_t>(vt+(uint64_t)i*8));
        if(fn<B||fn>=B+0x20000000ULL) continue;
        if(!Readable(fn,0x600)) continue;
        const unsigned char* p=(const unsigned char*)fn; bool hit=false;
        for(int o=0;o<0x800-4;o++){
            if(p[o]==0xD0 && p[o+1]==0x01 && p[o+2]==0x00 && p[o+3]==0x00){ hit=true; break; }
        }
        if(!hit) continue;
        fprintf(f,"IDX %d FN %p\n",i,(void*)fn);
        for(int o=0;o<0x200;o++){ fprintf(f,"%02X",p[o]); if((o&31)==31) fprintf(f,"\n"); }
        if(++found>=8) break;
    }
    fprintf(f,"FOUND=%d\nDONE\n",found); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
