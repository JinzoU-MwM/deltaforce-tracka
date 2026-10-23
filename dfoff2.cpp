#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
static bool Readable(uintptr_t a,size_t n){ if(!a)return false; MEMORY_BASIC_INFORMATION m{}; if(!VirtualQuery((LPCVOID)a,&m,sizeof(m)))return false; return m.State==MEM_COMMIT&&!(m.Protect&PAGE_NOACCESS)&&(uintptr_t)m.BaseAddress+m.RegionSize>=a+n; }
template<class T> static T R(uintptr_t a){ T v{}; if(Readable(a,sizeof(T))) memcpy(&v,(void*)a,sizeof(T)); return v; }
static uintptr_t D(uint64_t v){ return (uintptr_t)(v & 0xFFFFFFFFFFFFULL); }
static void FStr(uintptr_t addr,char* out,int cap){ out[0]=0; uintptr_t p=R<uintptr_t>(addr); int len=R<int>(addr+8); if(!p||len<=0||len>100) return; wchar_t w[128]{}; int n=len<120?len:120; if(!Readable(p,n*2)) return; memcpy(w,(void*)p,n*2); w[n]=0; WideCharToMultiByte(CP_UTF8,0,w,-1,out,cap,nullptr,nullptr); }
static DWORD WINAPI Th(LPVOID){
    uintptr_t B=(uintptr_t)GetModuleHandleW(nullptr);
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\off2.txt",L"w"); if(!f) return 1;
    uintptr_t w1=R<uintptr_t>(B+0x1D9E4E78), w2=R<uintptr_t>(B+0x1280AE68);
    fprintf(f,"BASE=%p W_core=%p W_dma=%p\n",(void*)B,(void*)w1,(void*)w2);
    for(int wi=0;wi<2;wi++){
        uintptr_t w=wi?w2:w1; if(!Readable(w,0x200)){ fprintf(f,"WORLD%d invalid\n",wi); continue; }
        uintptr_t lv1=D(R<uint64_t>(w+0xF8)), lv2=D(R<uint64_t>(w+0x108));
        uintptr_t gi1=D(R<uint64_t>(w+0x190)), gi2=D(R<uint64_t>(w+0x1B0));
        fprintf(f,"WORLD%d level[0xF8]=%p level[0x108]=%p gi[0x190]=%p gi[0x1B0]=%p\n",wi,(void*)lv1,(void*)lv2,(void*)gi1,(void*)gi2);
        uintptr_t lpA=D(R<uint64_t>(gi1+0x38)), lp=D(R<uint64_t>(lpA)), pc1=D(R<uint64_t>(lp+0x30));
        uintptr_t lpA2=D(R<uint64_t>(gi2+0x48)), lp2=D(R<uint64_t>(lpA2)), pc2=D(R<uint64_t>(lp2+0x38));
        fprintf(f,"  PCcore=%p PCdma=%p\n",(void*)pc1,(void*)pc2);
        uintptr_t cm1=D(R<uint64_t>(pc1+0x408)), cm2=D(R<uint64_t>(pc2+0x428));
        uintptr_t cmx[4]={cm1,cm2,D(R<uint64_t>(pc1+0x428)),D(R<uint64_t>(pc2+0x408))};
        const char* nms[4]={"PC1+408","PC2+428","PC1+428","PC2+408"};
        for(int c=0;c<4;c++){
            uintptr_t cm=cmx[c]; if(!Readable(cm,0x100)) continue;
            fprintf(f,"  CAM %s=%p\n",nms[c],(void*)cm);
            DWORD offs[4]={0x2DEC0,0x2DEF0,0x2F310,0x30870};
            for(int k=0;k<4;k++){
                DWORD o=offs[k]; float lx=R<float>(cm+o),ly=R<float>(cm+o+4),lz=R<float>(cm+o+8);
                float pi=R<float>(cm+o+0xC),ya=R<float>(cm+o+0x10),ro=R<float>(cm+o+0x14),fv=R<float>(cm+o+0x18);
                if(fabsf(lx)<1e7f&&fabsf(ly)<1e7f&&fabsf(lz)<1e7f&&fv>1&&fv<180)
                    fprintf(f,"    @%05X loc=(%.1f,%.1f,%.1f) rot=(%.1f,%.1f,%.1f) fov=%.1f\n",o,lx,ly,lz,pi,ya,ro,fv);
            }
        }
        uintptr_t lv=Readable(lv1,0x200)?lv1:lv2;
        uintptr_t arr=D(R<uint64_t>(lv+0x98)); int cnt=R<int>(lv+0x98+8);
        fprintf(f,"  LEVEL=%p arr=%p cnt=%d\n",(void*)lv,(void*)arr,cnt);
        if(cnt<0||cnt>200000) cnt=0;
        int shown=0;
        for(int i=0;i<cnt&&shown<25;i++){
            uintptr_t a=D(R<uint64_t>(arr+i*8)); if(!Readable(a,0x500)) continue;
            uintptr_t ps1=D(R<uint64_t>(a+0x390)), ps2=D(R<uint64_t>(a+0x3A0));
            uintptr_t r1=D(R<uint64_t>(a+0x180)), r2=D(R<uint64_t>(a+0x188));
            if(!Readable(ps1,0x100)&&!Readable(ps2,0x100)) continue;
            char n1[80]={0},n2[80]={0};
            if(Readable(ps1,0x500)) FStr(ps1+0x478,n1,80);
            if(Readable(ps2,0x500)) FStr(ps2+0x488,n2,80);
            float x1=R<float>(r1+0x168),y1=R<float>(r1+0x16C),z1=R<float>(r1+0x170);
            float x2=R<float>(r2+0x16C),y2=R<float>(r2+0x170),z2=R<float>(r2+0x174);
            int t1=R<int>(ps1+0x66C),c1=R<int>(ps1+0x670),t2=R<int>(ps2+0x67C),c2=R<int>(ps2+0x680);
            fprintf(f,"  A a=%p ps1=%p ps2=%p r1=%p r2=%p\n",(void*)a,(void*)ps1,(void*)ps2,(void*)r1,(void*)r2);
            fprintf(f,"    name1='%s' name2='%s'\n",n1,n2);
            fprintf(f,"    rel1=(%.1f,%.1f,%.1f) rel2=(%.1f,%.1f,%.1f)\n",x1,y1,z1,x2,y2,z2);
            fprintf(f,"    team1=%d camp1=%d team2=%d camp2=%d\n",t1,c1,t2,c2);
            shown++;
        }
    }
    fprintf(f,"DONE\n"); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
