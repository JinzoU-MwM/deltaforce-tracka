#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
static bool Readable(uintptr_t a,size_t n){ if(!a)return false; MEMORY_BASIC_INFORMATION m{}; if(!VirtualQuery((LPCVOID)a,&m,sizeof(m)))return false; return m.State==MEM_COMMIT&&!(m.Protect&PAGE_NOACCESS)&&(uintptr_t)m.BaseAddress+m.RegionSize>=a+n; }
template<class T> static T R(uintptr_t a){ T v{}; if(Readable(a,sizeof(T))) memcpy(&v,(void*)a,sizeof(T)); return v; }
static uintptr_t D(uint64_t v){ return (uintptr_t)(v & 0xFFFFFFFFFFFFULL); }
static bool NameAt(uintptr_t ps,uintptr_t off,char* out,int cap){
    out[0]=0; uintptr_t p=D(R<uint64_t>(ps+off)); int len=R<int>(ps+off+8);
    if(!p||len<2||len>24||!Readable(p,len*2)) return false;
    wchar_t w[32]{}; memcpy(w,(void*)p,len*2); w[len]=0;
    for(int i=0;i<len;i++){ if(w[i]<32||w[i]>126) return false; }
    WideCharToMultiByte(CP_UTF8,0,w,-1,out,cap,nullptr,nullptr); return out[0]!=0;
}
static DWORD WINAPI Th(LPVOID){
    uintptr_t B=(uintptr_t)GetModuleHandleW(nullptr);
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\squad.txt",L"w"); if(!f) return 1;
    uintptr_t w=D(R<uint64_t>(B+0x1D9E4E78));
    uintptr_t gs=D(R<uint64_t>(w+0x158));
    fprintf(f,"W=%p GS=%p\n",(void*)w,(void*)gs);
    if(Readable(gs,0x600)){
        for(DWORD go=0x300; go<=0x520; go+=8){
            uintptr_t arr=D(R<uint64_t>(gs+go)); int cnt=R<int>(gs+go+8);
            if(cnt<=0||cnt>200) continue;
            if(!Readable(arr,(size_t)cnt*8)) continue;
            int named=0; char first[80]="";
            for(int i=0;i<cnt;i++){
                uintptr_t ps=D(R<uint64_t>(arr+(uint64_t)i*8)); if(!Readable(ps,0x500)) continue;
                char nm[80];
                if(NameAt(ps,0x478,nm,80)||NameAt(ps,0x488,nm,80)){ named++; if(!first[0]) snprintf(first,sizeof(first),"%s",nm); }
            }
            if(named>0) fprintf(f,"GS+%03X arr=%p cnt=%d named=%d first='%s'\n",go,(void*)arr,cnt,named,first);
        }
    }
    fprintf(f,"DONE\n"); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
