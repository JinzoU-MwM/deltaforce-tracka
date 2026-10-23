#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
static bool Readable(uintptr_t a,size_t n){ if(!a)return false; MEMORY_BASIC_INFORMATION m{}; if(!VirtualQuery((LPCVOID)a,&m,sizeof(m)))return false; return m.State==MEM_COMMIT&&!(m.Protect&PAGE_NOACCESS)&&(uintptr_t)m.BaseAddress+m.RegionSize>=a+n; }
template<class T> static T R(uintptr_t a){ T v{}; if(Readable(a,sizeof(T))) memcpy(&v,(void*)a,sizeof(T)); return v; }
static uintptr_t D(uint64_t v){ return (uintptr_t)(v & 0xFFFFFFFFFFFFULL); }
static void FStr(uintptr_t addr,char* out,int cap){ out[0]=0; uintptr_t p=D(R<uint64_t>(addr)); int len=R<int>(addr+8); if(!p||len<=0||len>100) return; wchar_t w[128]{}; int n=len<120?len:120; if(!Readable(p,n*2)) return; memcpy(w,(void*)p,n*2); w[n]=0; WideCharToMultiByte(CP_UTF8,0,w,-1,out,cap,nullptr,nullptr); }
static void DumpPov(FILE* f,const char* tag,uintptr_t base){
    for(DWORD o=0x2C000;o<0x32000;o+=4){
        float lx=R<float>(base+o),ly=R<float>(base+o+4),lz=R<float>(base+o+8);
        float pi=R<float>(base+o+0xC),ya=R<float>(base+o+0x10),ro=R<float>(base+o+0x14),fv=R<float>(base+o+0x18);
        if(!(fv>25&&fv<150)) continue;
        if(!(fabsf(lx)>1000||fabsf(ly)>1000)) continue;
        if(!(fabsf(lx)<1e7&&fabsf(ly)<1e7&&fabsf(lz)<1e7)) continue;
        if(!(fabsf(pi)<90&&fabsf(ya)<360)) continue;
        fprintf(f,"    %s POV@%05X loc=(%.1f,%.1f,%.1f) rot=(%.1f,%.1f,%.1f) fov=%.1f\n",tag,o,lx,ly,lz,pi,ya,ro,fv);
        break;
    }
}
static DWORD WINAPI Th(LPVOID){
    uintptr_t B=(uintptr_t)GetModuleHandleW(nullptr);
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\off3.txt",L"w"); if(!f) return 1;
    uintptr_t w=D(R<uint64_t>(B+0x1D9E4E78));
    fprintf(f,"BASE=%p WORLD=%p\n",(void*)B,(void*)w);
    uintptr_t gs=D(R<uint64_t>(w+0x158));
    fprintf(f,"GS=%p\n",(void*)gs);
    uintptr_t gi=D(R<uint64_t>(w+0x190));
    uintptr_t lpA=D(R<uint64_t>(gi+0x38)), lp=D(R<uint64_t>(lpA)), pc=D(R<uint64_t>(lp+0x30));
    fprintf(f,"GI=%p PC=%p\n",(void*)gi,(void*)pc);
    if(Readable(gs,0x500)){
        for(DWORD go=0x380;go<=0x3C0;go+=8){
            uintptr_t a=D(R<uint64_t>(gs+go)); int n=R<int>(gs+go+8);
            if(n>=0&&n<5000) fprintf(f,"GS+%03X arr=%p cnt=%d\n",go,(void*)a,n);
        }
        for(DWORD go=0x380;go<=0x3C0;go+=8){
            uintptr_t a=D(R<uint64_t>(gs+go)); int n=R<int>(gs+go+8);
            if(!Readable(a,8)||n<=0||n>1000) continue;
            fprintf(f,"== PlayerArray @GS+%03X cnt=%d\n",go,n);
            for(int i=0;i<n&&i<40;i++){
                uintptr_t ps=D(R<uint64_t>(a+i*8)); if(!Readable(ps,0x100)) continue;
                char n1[80]={0},n2[80]={0},n3[80]={0};
                FStr(ps+0x478,n1,80); FStr(ps+0x488,n2,80); FStr(ps+0x498,n3,80);
                fprintf(f," PS[%d]=%p n478='%s' n488='%s' n498='%s' t66C=%d c670=%d t67C=%d c680=%d\n",
                    i,(void*)ps,n1,n2,n3,R<int>(ps+0x66C),R<int>(ps+0x670),R<int>(ps+0x67C),R<int>(ps+0x680));
            }
        }
    }
    if(Readable(pc,0x500)){
        fprintf(f,"== PC pointers\n");
        for(DWORD o=0x3B0;o<=0x450;o+=8){
            uintptr_t p=D(R<uint64_t>(pc+o));
            if(p>0x10000) fprintf(f," PC+%03X = %p\n",o,(void*)p);
        }
        const DWORD cand[]={0x3F0,0x408,0x410,0x418,0x420,0x428,0x430,0x438};
        for(int i=0;i<8;i++){
            uintptr_t cm=D(R<uint64_t>(pc+cand[i]));
            if(!Readable(cm,0x100)) continue;
            fprintf(f," CM@PC+%03X = %p\n",cand[i],(void*)cm);
            char tag[32]; wsprintfA(tag,"+%03X",cand[i]); DumpPov(f,tag,cm);
        }
        uintptr_t pawn=D(R<uint64_t>(pc+0x3F0));
        uintptr_t pawn2=D(R<uint64_t>(pc+0x408));
        fprintf(f," pawn3F0=%p pawn408=%p\n",(void*)pawn,(void*)pawn2);
        for(int k=0;k<2;k++){
            uintptr_t p=k?pawn2:pawn; if(!Readable(p,0x400)) continue;
            uintptr_t root=D(R<uint64_t>(p+0x180));
            float x=R<float>(root+0x168),y=R<float>(root+0x16C),z=R<float>(root+0x170);
            char nm[80]={0}; uintptr_t ps=D(R<uint64_t>(p+0x390)); if(Readable(ps,0x500)) FStr(ps+0x478,nm,80);
            fprintf(f," %s p=%p ps=%p name='%s' root=%p rel168=(%.1f,%.1f,%.1f)\n",k?"pawn408":"pawn3F0",(void*)p,(void*)ps,nm,(void*)root,x,y,z);
        }
    }
    fprintf(f,"DONE\n"); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
