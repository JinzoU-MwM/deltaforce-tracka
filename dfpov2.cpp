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
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\pov2.txt",L"w"); if(!f) return 1;
    uintptr_t w=D(R<uint64_t>(B+0x1D9E4E78));
    uintptr_t gi=D(R<uint64_t>(w+0x190));
    uintptr_t lpA=D(R<uint64_t>(gi+0x38)), lp=D(R<uint64_t>(lpA)), pc=D(R<uint64_t>(lp+0x30));
    uintptr_t cm=D(R<uint64_t>(pc+0x408));
    uintptr_t pawn=D(R<uint64_t>(pc+0x3F0));
    uintptr_t root=D(R<uint64_t>(pawn+0x180));
    float px=R<float>(root+0x168),py=R<float>(root+0x16C),pz=R<float>(root+0x170);
    fprintf(f,"PC=%p CM=%p pawn_rel=(%.0f,%.0f,%.0f)\n",(void*)pc,(void*)cm,px,py,pz);
    if(!Readable(cm,0x100)){ fprintf(f,"CM invalid\n"); fclose(f); return 0; }
    DWORD offs[]={0x400,0x17E0,0x2C00,0x2DF00,0x2F320,0x2DEF0,0x2F310,0x3F0,0x17D0,0x2BF0};
    for(int i=0;i<10;i++){
        DWORD o=offs[i];
        float lx=R<float>(cm+o),ly=R<float>(cm+o+4),lz=R<float>(cm+o+8);
        float pi=R<float>(cm+o+0xC),ya=R<float>(cm+o+0x10),ro=R<float>(cm+o+0x14),fv=R<float>(cm+o+0x18);
        float dx=lx-px,dy=ly-py,dz=lz-pz; float d=sqrtf(dx*dx+dy*dy+dz*dz);
        fprintf(f,"@%05X loc=(%.1f,%.1f,%.1f) rot=(%.2f,%.2f,%.2f) fov=%.1f dist=%.0f\n",o,lx,ly,lz,pi,ya,ro,fv,d);
    }
    fprintf(f,"DefaultFOV @398=%.1f @3A8=%.1f\n",R<float>(cm+0x398),R<float>(cm+0x3A8));
    fprintf(f,"DONE\n"); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
