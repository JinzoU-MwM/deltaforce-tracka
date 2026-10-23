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
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\pov.txt",L"w"); if(!f) return 1;
    uintptr_t w=D(R<uint64_t>(B+0x1D9E4E78));
    uintptr_t gi=D(R<uint64_t>(w+0x190));
    uintptr_t lpA=D(R<uint64_t>(gi+0x38)), lp=D(R<uint64_t>(lpA)), pc=D(R<uint64_t>(lp+0x30));
    uintptr_t pawn=D(R<uint64_t>(pc+0x3F0));
    uintptr_t root=D(R<uint64_t>(pawn+0x180));
    float sx=R<float>(root+0x168), sy=R<float>(root+0x16C), sz=R<float>(root+0x170);
    fprintf(f,"B=%p W=%p PC=%p pawn=%p self=(%.0f,%.0f,%.0f)\n",(void*)B,(void*)w,(void*)pc,(void*)pawn,sx,sy,sz);
    DWORD ptrs[]={0x408,0x410,0x400,0x3F8,0x418,0x420};
    for(int k=0;k<6;k++){
        uintptr_t cm=D(R<uint64_t>(pc+ptrs[k])); if(!Readable(cm,0x100)) continue;
        fprintf(f,"CM PC+%03X=%p\n",ptrs[k],(void*)cm);
        int hits=0;
        for(DWORD o=0x20000;o<0x34000;o+=4){
            float lx=R<float>(cm+o),ly=R<float>(cm+o+4),lz=R<float>(cm+o+8);
            float pi=R<float>(cm+o+0xC),ya=R<float>(cm+o+0x10),ro=R<float>(cm+o+0x14),fv=R<float>(cm+o+0x18);
            if(!(fv>20&&fv<160)) continue;
            if(!(fabsf(lx)<1e7&&fabsf(ly)<1e7&&fabsf(lz)<1e7)) continue;
            if(!(fabsf(pi)<=90&&fabsf(ya)<=360)) continue;
            float dx=lx-sx,dy=ly-sy,dz=lz-sz; float d=(float)sqrtf(dx*dx+dy*dy+dz*dz);
            if(d<2000.f){
                fprintf(f,"  POV@%05X d=%.0f loc=(%.0f,%.0f,%.0f) rot=(%.1f,%.1f,%.1f) fov=%.0f\n",o,d,lx,ly,lz,pi,ya,ro,fv);
                if(++hits>=12) break;
            }
        }
        fprintf(f,"  hits=%d\n",hits);
    }
    fprintf(f,"DONE\n"); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
