// dfenc — probe enkripsi posisi + kamera (build 1.14.0.772)
// Jalur: (A) dump total aktor 0..0x900 float triplet; (B) rantai capsule/root/PS
//        (C) cari decrypt di PS/pawn: bandingkan rel vs nilai di sekitar
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>

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

// "sane world coord" — koordinat map Delta Force ~ +-600000 cm, Z -20000..20000
static bool Sane(float x,float y,float z){
    if(!(x==x&&y==y&&z==z)) return false;
    if(fabsf(x)>600000.f||fabsf(y)>600000.f) return false;
    if(z<-20000.f||z>20000.f) return false;
    if(fabsf(x)<1.f&&fabsf(y)<1.f) return false;   // origin/near-origin bukan posisi aktor nyata
    return true;
}
static bool Sane2(float a,float b){ return fabsf(a)<600000.f&&fabsf(b)<600000.f&&(a*a+b*b)>1.f; }

static void Hexdump(const char* label, uintptr_t base, size_t len, FILE* f){
    fprintf(f,"  %s @%p (%zu)\n",label,(void*)base,len);
    for(size_t i=0;i<len;i+=16){
        unsigned char b[16]{}; if(!Readable(base+i,16)) break; memcpy(b,(void*)(base+i),16);
        fprintf(f,"   %04zX ",i);
        for(int j=0;j<16;j++) fprintf(f,"%02X ",b[j]);
        fprintf(f," |");
        for(int j=0;j<16;j++){ unsigned char c=b[j]; fputc((c>=0x20&&c<0x7f)?c:'.',f); }
        fprintf(f,"|\n");
    }
}

static DWORD WINAPI Th(LPVOID){
    uintptr_t B=(uintptr_t)GetModuleHandleW(nullptr); g_blocks=B+0x1E5A7788;
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\enc.txt",L"w"); if(!f) return 1;
    fprintf(f,"BASE=%p blocks=%p\n",(void*)B,(void*)g_blocks);
    uintptr_t w=D(R<uint64_t>(B+0x1D9E4E78));
    uintptr_t lv=D(R<uint64_t>(w+0xF8));
    uintptr_t arr=D(R<uint64_t>(lv+0x98)); int cnt=R<int>(lv+0x98+8);
    fprintf(f,"W=%p LV=%p arr=%p cnt=%d\n",(void*)w,(void*)lv,(void*)arr,cnt);
    if(cnt<0||cnt>200000){ fprintf(f,"BADCNT\nDONE\n"); fclose(f); return 0; }

    int chars=0, plain=0, garbled=0;
    for(int i=0;i<cnt && chars<40;i++){
        uintptr_t a=D(R<uint64_t>(arr+(uint64_t)i*8));
        if(!Readable(a,0x900)) continue;
        char cn[104]; ClassName(a,cn,104);
        if(!SContains(cn,"BP_DFM_Character")) continue;
        uintptr_t ps=D(R<uint64_t>(a+0x390));
        if(!Readable(ps,0x800)) continue;
        char nm[64]={0}; FStr(ps,0x478,nm,64); if(!nm[0]) FStr(ps,0x488,nm,64);
        if(!nm[0]) continue;
        uintptr_t root=D(R<uint64_t>(a+0x180));
        float rx=R<float>(root+0x168),ry=R<float>(root+0x16C),rz=R<float>(root+0x170);
        bool ok=Sane(rx,ry,rz);
        if(ok) plain++; else garbled++;
        fprintf(f,"\n=== [%d] %s a=%p root=%p rel=(%.1f,%.1f,%.1f) %s\n",chars,nm,(void*)a,(void*)root,rx,ry,rz,ok?"PLAIN":"GARBLED");
        chars++;

        // A) sapu seluruh aktor: cari triplet yang sane dan dekat Z rel
        fprintf(f,"  -- A: sane triplets in actor 0..0x900\n");
        int hit=0;
        for(DWORD o=0;o<0x900;o+=4){
            float x=R<float>(a+o),y=R<float>(a+o+4),z=R<float>(a+o+8);
            if(!Sane(x,y,z)) continue;
            fprintf(f,"    A+%03X (%.1f,%.1f,%.1f)\n",o,x,y,z);
            if(++hit>=12) break;
        }
        if(ok) fprintf(f,"    (expect A+168 present)\n");

        // B) komponen: capsule / skeleton / movement — cari anak SceneComponent
        fprintf(f,"  -- B: component pointers\n");
        for(DWORD o=0x180;o<0x480;o+=8){
            uintptr_t p=D(R<uint64_t>(a+o));
            if(!Readable(p,0x30)) continue;
            char cc[104]; ClassName(p,cc,104);
            if(SContains(cc,"?")) continue;
            fprintf(f,"    A+%03X = %p cls=%s\n",o,(void*)p,cc);
        }

        // C) PlayerState: sapu triplet sane
        fprintf(f,"  -- C: sane triplets in PS 0..0x800\n");
        hit=0;
        for(DWORD o=0;o<0x800;o+=4){
            float x=R<float>(ps+o),y=R<float>(ps+o+4),z=R<float>(ps+o+8);
            if(!Sane(x,y,z)) continue;
            fprintf(f,"    PS+%03X (%.1f,%.1f,%.1f)\n",o,x,y,z);
            if(++hit>=10) break;
        }

        // D) hexdump root+0x140..0x1C0 (lihat struktur mentah sekitar rel)
        Hexdump("root[0x140..0x1C0]", root+0x140, 0x80, f);

        // E) hexdump a+0x3A0..0x420 (di sekitar PS ptr — cari encrypted blob)
        Hexdump("actor[0x380..0x400]", a+0x380, 0x80, f);
    }
    fprintf(f,"\nCHARS=%d PLAIN=%d GARBLED=%d\nDONE\n",chars,plain,garbled);
    fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
