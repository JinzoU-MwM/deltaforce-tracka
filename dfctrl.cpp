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
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\ctrl.txt",L"w"); if(!f) return 1;
    uintptr_t w=D(R<uint64_t>(B+0x1D9E4E78));
    uintptr_t gi=D(R<uint64_t>(w+0x190));
    uintptr_t lpA=D(R<uint64_t>(gi+0x38)), lp=D(R<uint64_t>(lpA)), pc=D(R<uint64_t>(lp+0x30));
    fprintf(f,"PC=%p\n",(void*)pc);
    for(DWORD o=0x3B0;o<=0x450;o+=8){
        uintptr_t p=D(R<uint64_t>(pc+o));
        float a=R<float>(pc+o),b=R<float>(pc+o+4),c=R<float>(pc+o+8);
        fprintf(f,"PC+%03X ptr=%p  f=(%.2f,%.2f,%.2f)\n",o,(void*)p,a,b,c);
    }
    uintptr_t pawn=D(R<uint64_t>(pc+0x3F0));
    uintptr_t pawn2=D(R<uint64_t>(pc+0x400));
    fprintf(f,"pawn3F0=%p pawn400=%p\n",(void*)pawn,(void*)pawn2);
    for(int k=0;k<2;k++){
        uintptr_t p=k?pawn2:pawn; if(!Readable(p,0x400)) { fprintf(f,"pawn%d invalid\n",k); continue; }
        uintptr_t root=D(R<uint64_t>(p+0x180));
        float x=R<float>(root+0x168),y=R<float>(root+0x16C),z=R<float>(root+0x170);
        float rp=R<float>(root+0x178),ry=R<float>(root+0x17C),rr=R<float>(root+0x180);
        fprintf(f,"pawn%d=%p root=%p rel=(%.0f,%.0f,%.0f) rotr=(%.1f,%.1f,%.1f)\n",k,(void*)p,(void*)root,x,y,z,rp,ry,rr);
        float cp=R<float>(pc+0x3E0),cy=R<float>(pc+0x3E4),cr=R<float>(pc+0x3E8);
        fprintf(f,"  PC+3E0 rot=(%.1f,%.1f,%.1f)\n",cp,cy,cr);
    }
    fprintf(f,"DONE\n"); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
