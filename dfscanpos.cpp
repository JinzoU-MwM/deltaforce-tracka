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
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\scanpos.txt",L"w"); if(!f) return 1;
    uintptr_t w=D(R<uint64_t>(B+0x1D9E4E78)); uintptr_t lv=D(R<uint64_t>(w+0xF8));
    uintptr_t arr=D(R<uint64_t>(lv+0x98)); int cnt=R<int>(lv+0x98+8);
    fprintf(f,"cnt=%d\n",cnt); int done=0;
    for(int i=0;i<cnt&&done<6;i++){
        uintptr_t a=D(R<uint64_t>(arr+(uint64_t)i*8)); if(!Readable(a,0x400)) continue;
        uintptr_t c=D(R<uint64_t>(a+0x08)); if(!Readable(c,0x20)) continue;
        // crude class check via known pointer range: just take actors with ps
        uintptr_t ps=D(R<uint64_t>(a+0x390)); if(!Readable(ps,0x500)) continue;
        char nm[64]={0}; uintptr_t np=D(R<uint64_t>(ps+0x478)); int nl=R<int>(ps+0x478+8);
        if(np&&nl>1&&nl<20&&Readable(np,nl*2)){ wchar_t ww[24]{}; memcpy(ww,(void*)np,nl*2); WideCharToMultiByte(CP_UTF8,0,ww,-1,nm,64,nullptr,nullptr);} 
        if(!nm[0]) continue;
        uintptr_t root=D(R<uint64_t>(a+0x180));
        float rx=R<float>(root+0x168),ry=R<float>(root+0x16C),rz=R<float>(root+0x170);
        fprintf(f,"== %s a=%p rel=(%.0f,%.0f,%.0f)\n",nm,(void*)a,rx,ry,rz);
        int hits=0;
        for(DWORD o=0;o<0x800;o+=4){
            float x=R<float>(a+o),y=R<float>(a+o+4),z=R<float>(a+o+8);
            if(!(fabsf(x)<600000.f&&fabsf(y)<600000.f&&fabsf(z)<20000.f)) continue;
            if(fabsf(x)<10.f&&fabsf(y)<10.f) continue;
            if(fabsf(z)<0.01f) continue;
            fprintf(f,"  +%03X (%.0f,%.0f,%.0f)\n",o,x,y,z);
            if(++hits>=10) break;
        }
        done++;
    }
    fprintf(f,"DONE\n"); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
