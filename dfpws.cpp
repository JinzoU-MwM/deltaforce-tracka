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
static bool Canon(uintptr_t p){ return p>0x10000 && p<0x7FFFFFFFFFFFULL; }
static uint32_t FindNameIdx(uintptr_t gblocks,const char* target,int* ok){
    int tl=(int)strlen(target); *ok=0;
    wchar_t wt[64]; for(int i=0;i<tl;i++) wt[i]=(wchar_t)(unsigned char)target[i]; wt[tl]=0;
    unsigned char xt[64]; { uint8_t k=XorKey((size_t)tl); for(int i=0;i<tl;i++) xt[i]=(unsigned char)target[i]^k; }
    for(int b=0;b<0x800;b++){
        uintptr_t blk=R<uintptr_t>(gblocks+(uint64_t)b*8); if(!Readable(blk,0x40)) continue;
        size_t sz=0x80000; if(!Readable(blk,sz)) sz=0x40000; if(!Readable(blk,sz)) continue;
        const unsigned char* p=(const unsigned char*)blk;
        for(size_t o=0;o+(size_t)tl*2<sz;o++){
            if(memcmp(p+o,wt,(size_t)tl*2)==0){
                for(int d=-2;d<=2;d++){ long long e=(long long)o+d; if(e<0) continue; uint32_t idx=(uint32_t)(b*0x40000 + e/2);
                    char nm[104]={0}; FNameToBuf(idx,nm,104); if(strcmp(nm,target)==0){ *ok=1; return idx; } }
            }
        }
        for(size_t o=0;o+(size_t)tl<sz;o++){
            if(memcmp(p+o,xt,(size_t)tl)==0){
                for(int d=-2;d<=2;d++){ long long e=(long long)o+d; if(e<0) continue; uint32_t idx=(uint32_t)(b*0x40000 + e/2);
                    char nm[104]={0}; FNameToBuf(idx,nm,104); if(strcmp(nm,target)==0){ *ok=1; return idx; } }
            }
        }
    }
    return 0;
}
static bool IsPropName(const char* n){ return strstr(n,"Property")||strstr(n,"Bool")||strstr(n,"Struct")||strstr(n,"Object"); }
static DWORD WINAPI Th(LPVOID){
    uintptr_t B=(uintptr_t)GetModuleHandleW(nullptr); g_blocks=B+0x1E5A7788;
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\pws.txt",L"w"); if(!f) return 1;
    int ok=0; uint32_t IDX=FindNameIdx(g_blocks,"GameplayStatics",&ok);
    if(!ok){ fclose(f); return 0; }
    unsigned char pat[4]={(unsigned char)IDX,(unsigned char)(IDX>>8),(unsigned char)(IDX>>16),(unsigned char)(IDX>>24)};
    uintptr_t p=0; MEMORY_BASIC_INFORMATION m{}; uintptr_t gsClass=0;
    while(!gsClass && VirtualQuery((LPCVOID)p,&m,sizeof(m))==sizeof(m)){
        uintptr_t base=(uintptr_t)m.BaseAddress; SIZE_T sz=m.RegionSize;
        if((m.State==MEM_COMMIT)&&!(m.Protect&PAGE_GUARD)&&(m.Protect==PAGE_READWRITE)&&sz>=0x1000&&sz<0x40000000){
            const unsigned char* b=(const unsigned char*)base; size_t i=0;
            while(i+4<=sz){
                const unsigned char* q=(const unsigned char*)memchr(b+i,pat[0],sz-i); if(!q) break;
                size_t off=(size_t)(q-b);
                if(off+4<=sz&&memcmp(q,pat,4)==0){
                    uintptr_t obj=base+off-0x1C;
                    if(Canon(obj)&&Readable(obj,0x40)){
                        uintptr_t vt=R<uint64_t>(obj); uintptr_t c=D(R<uint64_t>(obj+0x08));
                        if(vt>=B&&vt<B+0x20000000ULL&&Canon(c)&&Readable(c,0x40)){
                            char cn[80]={0}; FNameToBuf(R<uint32_t>(c+0x1C),cn,80);
                            if(strcmp(cn,"Class")==0){ char nm[104]={0}; FNameToBuf(R<uint32_t>(obj+0x1C),nm,104); if(strcmp(nm,"GameplayStatics")==0){ gsClass=obj; break; } }
                        }
                    }
                }
                i=off+1;
            }
        }
        uintptr_t next=base+sz; if(next<=p) break; p=next;
    }
    if(!gsClass){ fprintf(f,"no gs\n"); fclose(f); return 0; }
    uintptr_t pw=0; uintptr_t cur=D(R<uint64_t>(gsClass+0x58));
    for(int n=0;Canon(cur)&&Readable(cur,0x40)&&n<400;n++){
        char fn[104]={0}; FNameToBuf(R<uint32_t>(cur+0x1C),fn,104);
        if(strcmp(fn,"ProjectWorldToScreen")==0){ pw=cur; break; }
        uintptr_t nx=D(R<uint64_t>(cur+0x28)); if(nx==cur) break; cur=nx;
    }
    fprintf(f,"GSCLASS=%p PWS=%p\n",(void*)gsClass,(void*)pw);
    if(pw){
        fprintf(f,"qwords A0..C0:\n");
        for(DWORD o=0xA0;o<=0xC0;o+=8) fprintf(f," +%03X = %016llX\n",o,(unsigned long long)R<uint64_t>(pw+o));
        const unsigned char* bp=(const unsigned char*)pw;
        fprintf(f,"bytes 90..C0: "); for(DWORD o=0x90;o<0xC0;o++) fprintf(f,"%02X",bp[o]); fprintf(f,"\n");
        fprintf(f,"qwords 1C0..1E8:\n");
        for(DWORD o=0x1C0;o<=0x1E8;o+=8) fprintf(f," +%03X = %016llX\n",o,(unsigned long long)R<uint64_t>(pw+o));
    }
    uintptr_t hits[12]={0x1412C5B7CULL,0x14145813AULL,0x1414581AFULL,0x14145821CULL,0x141458289ULL,0x141BE6F3EULL,0x141BE6FD1ULL,0x141BE70A5ULL,0x141DC80AEULL,0x141E31C85ULL,0x14222BCA7ULL,0x142231658ULL};
    for(int k=0;k<12;k++){
        uintptr_t a=hits[k]; if(!Readable(a,96)){ fprintf(f,"HIT %p unreadable\n",(void*)a); continue; }
        const unsigned char* h=(const unsigned char*)a;
        fprintf(f,"HIT %p: ",(void*)a); for(int i=0;i<96;i++) fprintf(f,"%02X",h[i]); fprintf(f,"\n");
    }
    fprintf(f,"DONE\n"); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
