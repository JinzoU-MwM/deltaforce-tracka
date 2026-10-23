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
static bool ObjOK(uintptr_t o,char* nm,int cap){
    if(!Canon(o)||!Readable(o,0x40)) return false;
    uintptr_t c=D(R<uint64_t>(o+0x08)); if(!Canon(c)||!Readable(c,0x40)) return false;
    uint32_t ni=R<uint32_t>(c+0x1C); if(ni==0||ni>0x2000000) return false;
    FNameToBuf(ni,nm,cap); if(nm[0]=='?'||nm[0]==0) return false;
    int l=(int)strlen(nm); if(l<2||l>80) return false;
    for(int i=0;i<l;i++){ unsigned char ch=nm[i]; if(ch<32||ch>126) return false; }
    return true;
}
static int ArrayCount(uintptr_t P,int stride){
    int n=0,bad=0;
    for(int i=0;i<4000000;i++){
        uintptr_t o=D(R<uint64_t>(P+(size_t)i*stride));
        char nm[80];
        if(o==0){ if(++bad>64) break; continue; }
        if(ObjOK(o,nm,80)){ n=i+1; bad=0; }
        else { if(++bad>64) break; }
    }
    return n;
}
static DWORD WINAPI Th(LPVOID){
    uintptr_t B=(uintptr_t)GetModuleHandleW(nullptr); g_blocks=B+0x1E5A7788;
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\gobj2.txt",L"w"); if(!f) return 1;
    auto dos=(PIMAGE_DOS_HEADER)B; auto nt=(PIMAGE_NT_HEADERS)(B+dos->e_lfanew); auto sec=IMAGE_FIRST_SECTION(nt);
    fprintf(f,"BASE=%p\n",(void*)B);
    int cand=0;
    for(WORD si=0; si<nt->FileHeader.NumberOfSections; ++si,++sec){
        if(!(sec->Characteristics&IMAGE_SCN_MEM_EXECUTE)) continue;
        uintptr_t va=B+sec->VirtualAddress; size_t sz=sec->Misc.VirtualSize?sec->Misc.VirtualSize:sec->SizeOfRawData;
        if(!Readable(va,sz)) continue;
        const BYTE* p=(const BYTE*)va;
        fprintf(f,"TEXT %s %p sz=%zu\n",sec->Name,(void*)va,sz);
        for(size_t i=0;i+10<sz;i++){
            if(p[i]!=0x48) continue;
            BYTE r=p[i+1];
            if(r!=0x8B) continue;
            BYTE modrm=p[i+2];
            // mov reg,[rip+disp32]: modrm 05/0D/15/1D/25/2D/35/3D
            if((modrm&0xC7)!=0x05) continue;
            int32_t disp=*(int32_t*)(p+i+3);
            uintptr_t tgt=va+i+7+(intptr_t)disp;
            // next insn: 48 8B 0C (modrm with SIB)
            const BYTE* q=p+i+7;
            if(q[0]!=0x48||q[1]!=0x8B) continue;
            BYTE m2=q[2];
            if((m2&0xC0)!=0x00 && (m2&0xC0)!=0x40) continue;
            if(m2!=0x0C && m2!=0x0D && m2!=0x0E && m2!=0x0F && m2!=0x4C && m2!=0x4D) continue;
            uintptr_t P=R<uintptr_t>(tgt);
            uintptr_t cands[2]={P,tgt};
            for(int k=0;k<2;k++){
                uintptr_t A=cands[k]; if(!Canon(A)||!Readable(A,0x40)) continue;
                char n0[80],n1[80],n2[80];
                uintptr_t o0=D(R<uint64_t>(A)),o1=D(R<uint64_t>(A+8)),o2=D(R<uint64_t>(A+16));
                if(!ObjOK(o0,n0,80)) continue;
                if(!ObjOK(o1,n1,80)&&!ObjOK(o2,n2,80)) continue;
                int cnt=ArrayCount(A,24); if(cnt<1000) cnt=ArrayCount(A,16);
                if(cnt<1000) continue;
                fprintf(f,"HIT insn=%p tgt=%p A=%p cnt=%d o0=%s o1=%s\n",(void*)(va+i),(void*)tgt,(void*)A,cnt,n0,n1);
                cand++;
            }
            if(cand>=10) break;
        }
        if(cand>=10) break;
    }
    fprintf(f,"CAND=%d\nDONE\n",cand); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
