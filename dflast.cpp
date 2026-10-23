#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
static bool Readable(uintptr_t a,size_t n){ if(!a)return false; MEMORY_BASIC_INFORMATION m{}; if(!VirtualQuery((LPCVOID)a,&m,sizeof(m)))return false; return m.State==MEM_COMMIT&&!(m.Protect&PAGE_NOACCESS)&&(uintptr_t)m.BaseAddress+m.RegionSize>=a+n; }
template<class T> static T R(uintptr_t a){ T v{}; if(Readable(a,sizeof(T))) memcpy(&v,(void*)a,sizeof(T)); return v; }
static uintptr_t D(uint64_t v){ return (uintptr_t)(v & 0xFFFFFFFFFFFFULL); }
static void SCopy(char* d,int c,const char* s){ int i=0; for(;s[i]&&i<c-1;i++) d[i]=s[i]; d[i]=0; }
static bool SContains(const char* h,const char* n){ if(!*n) return true; for(;*h;h++){ const char* a=h,*b=n; while(*a&&*b&&*a==*b){a++;b++;} if(!*b) return true; } return false; }
static uint8_t XorKey(size_t len){ uint32_t r9d=(uint32_t)len, rem=r9d%9; uint8_t b=(uint8_t)r9d,k=0;
 switch(rem){ case 0:k=(b&0x1F)+0x80;k=(uint8_t)(k+b);k|=0x7F;break; case 1:k=(uint8_t)(b^0xDF);k=(uint8_t)(k+0x80);k=(uint8_t)(k+b);k|=0x7F;break;
 case 2:k=(uint8_t)(b|0xCF);k=(uint8_t)(k+0x80);k=(uint8_t)(k+b);k|=0x7F;break; case 3:{int8_t s=(int8_t)b;k=(uint8_t)(s*0x21);k=(uint8_t)(k+0x80);k|=0x7F;break;}
 case 4:k=(uint8_t)(b>>2);k=(uint8_t)(k+0x80);k=(uint8_t)(k+b);k|=0x7F;break; case 5:{uint8_t t=(uint8_t)(b-0x29);k=(uint8_t)(t+t);k=(uint8_t)(k+t);k|=0x7F;break;}
 case 6:k=(uint8_t)((b<<2)|0x05);k=(uint8_t)(k+0x80);k=(uint8_t)(k+b);k|=0x7F;break; case 7:k=(uint8_t)((b>>4)|0x07);k=(uint8_t)(k+0x80);k=(uint8_t)(k+b);k|=0x7F;break;
 default:k=(uint8_t)(b^0x40);k=(uint8_t)(k+0x80);k=(uint8_t)(k+b);k|=0x7F;break;} return k; }
static uintptr_t g_blocks=0;
static void FNameToBuf(uint32_t idx,char* out,int cap){ SCopy(out,cap,"?"); if(!g_blocks)return; uintptr_t blk=R<uintptr_t>(g_blocks+(uint64_t)(idx>>18)*8); if(!Readable(blk,8))return;
 uintptr_t ent=blk+(uint64_t)(idx&0x3FFFF)*2; uint16_t hdr=R<uint16_t>(ent); uint32_t len=hdr>>6; if(!len||len>1023)return;
 if(hdr&1){ wchar_t w[128]; uint32_t n=len<120?len:120; if(Readable(ent+2,n*2)){ memcpy(w,(void*)(ent+2),n*2); w[n]=0; WideCharToMultiByte(CP_UTF8,0,w,-1,out,cap,nullptr,nullptr);} }
 else { char s[128]; uint32_t n=len<120?len:120; if(Readable(ent+2,n)){ memcpy(s,(void*)(ent+2),n); uint8_t k=XorKey(len); for(uint32_t i=0;i<n;i++) s[i]=(char)((uint8_t)s[i]^k); s[n]=0; SCopy(out,cap,s);} } }
static void ClassName(uintptr_t a,char* out,int cap){ uintptr_t c=D(R<uint64_t>(a+0x08)); if(!Readable(c,0x20)){SCopy(out,cap,"?");return;} FNameToBuf(R<uint32_t>(c+0x1C),out,cap); }
static void FStr(uintptr_t ps,uintptr_t off,char* out,int cap){ out[0]=0; if(!Readable(ps+off,0x10))return; uintptr_t p=D(R<uint64_t>(ps+off)); int len=R<int>(ps+off+8); if(!p||len<2||len>24||!Readable(p,len*2))return; wchar_t w[32]{}; memcpy(w,(void*)p,len*2); w[len]=0; WideCharToMultiByte(CP_UTF8,0,w,-1,out,cap,nullptr,nullptr); }
static DWORD WINAPI Th(LPVOID){
    uintptr_t B=(uintptr_t)GetModuleHandleW(nullptr); g_blocks=B+0x1E5A7788;
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\last.txt",L"w"); if(!f) return 1;
    uintptr_t w=D(R<uint64_t>(B+0x1D9E4E78));
    uintptr_t lv=D(R<uint64_t>(w+0xF8));
    uintptr_t arr=D(R<uint64_t>(lv+0x98)); int cnt=R<int>(lv+0x98+8);
    fprintf(f,"W=%p LV=%p arr=%p cnt=%d\n",(void*)w,(void*)lv,(void*)arr,cnt);
    if(cnt<0||cnt>200000) cnt=0; int shown=0;
    for(int i=0;i<cnt;i++){
        uintptr_t a=D(R<uint64_t>(arr+(uint64_t)i*8)); if(!Readable(a,0x400)) continue;
        char cn[104]; ClassName(a,cn,104);
        if(!SContains(cn,"Character")&&!SContains(cn,"Pawn")) continue;
        uintptr_t ps1=D(R<uint64_t>(a+0x390)), ps2=D(R<uint64_t>(a+0x3A0));
        uintptr_t root=D(R<uint64_t>(a+0x180));
        float x=R<float>(root+0x168),y=R<float>(root+0x16C),z=R<float>(root+0x170);
        float r0x=R<float>(a+0xBC),r0y=R<float>(a+0xC0),r0z=R<float>(a+0xC4);
        float r1x=R<float>(a+0x98),r1y=R<float>(a+0x9C),r1z=R<float>(a+0xA0);
        float r2x=R<float>(a+0x78),r2y=R<float>(a+0x7C),r2z=R<float>(a+0x80);
        uintptr_t mv=D(R<uint64_t>(a+0x3F0)); if(!Readable(mv,0x400)) mv=D(R<uint64_t>(a+0x3E8));
        float mx=R<float>(mv+0x2B0),my=R<float>(mv+0x2B4),mz=R<float>(mv+0x2B8);
        char n1[64]={0},n2[64]={0},n3[64]={0},n4[64]={0};
        FStr(ps1,0x478,n1,64); FStr(ps2,0x478,n2,64); FStr(ps1,0x488,n3,64); FStr(ps2,0x488,n4,64);
        fprintf(f,"A %p cls=%s name='%s' rel=(%.0f,%.0f,%.0f) mv=%p last=(%.0f,%.0f,%.0f)\n",
            (void*)a,cn,n1[0]?n1:n3,x,y,z,(void*)mv,mx,my,mz);
        if(++shown>=120) break;
    }
    fprintf(f,"SHOWN=%d\nDONE\n",shown); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
