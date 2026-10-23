#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static bool Readable(uintptr_t a,size_t n){ if(!a)return false; MEMORY_BASIC_INFORMATION m{}; if(!VirtualQuery((LPCVOID)a,&m,sizeof(m)))return false; return m.State==MEM_COMMIT&&!(m.Protect&PAGE_NOACCESS)&&(uintptr_t)m.BaseAddress+m.RegionSize>=a+n; }
static DWORD WINAPI Th(LPVOID){
    uintptr_t B=(uintptr_t)GetModuleHandleW(nullptr);
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\str.txt",L"w"); if(!f) return 1;
    auto dos=(PIMAGE_DOS_HEADER)B; auto nt=(PIMAGE_NT_HEADERS)(B+dos->e_lfanew); auto sec=IMAGE_FIRST_SECTION(nt);
    const char* needle="ProcessEvent"; int nl=12;
    uintptr_t found[32]; int nf=0;
    for(WORD si=0;si<nt->FileHeader.NumberOfSections&&nf<32;++si,++sec){
        uintptr_t va=B+sec->VirtualAddress; size_t sz=sec->Misc.VirtualSize?sec->Misc.VirtualSize:sec->SizeOfRawData;
        if(!Readable(va,sz)) continue; const char* p=(const char*)va;
        for(size_t i=0;i+nl<sz;i++){ if(memcmp(p+i,needle,nl)==0){ found[nf++]=va+i; fprintf(f,"STR %p sec=%s\n",(void*)(va+i),sec->Name); if(nf>=32) break; } }
    }
    // xrefs: scan exec sections for rip-relative lea/mov targeting found strings
    sec=IMAGE_FIRST_SECTION(nt);
    int xr=0;
    for(WORD si=0;si<nt->FileHeader.NumberOfSections&&xr<40;++si,++sec){
        if(!(sec->Characteristics&IMAGE_SCN_MEM_EXECUTE)) continue;
        uintptr_t va=B+sec->VirtualAddress; size_t sz=sec->Misc.VirtualSize?sec->Misc.VirtualSize:sec->SizeOfRawData;
        if(!Readable(va,sz)) continue; const unsigned char* p=(const unsigned char*)va;
        for(size_t i=0;i+7<sz&&xr<40;i++){
            if(p[i]!=0x48) continue;
            if(!(p[i+1]==0x8D||p[i+1]==0x8B)) continue;
            unsigned char m=p[i+2];
            if((m&0xC7)!=0x05) continue; // RIP-relative
            int32_t disp=*(int32_t*)(p+i+3);
            uintptr_t tgt=va+i+7+(intptr_t)disp;
            for(int k=0;k<nf;k++){
                if(tgt>=found[k]&&tgt<found[k]+(uintptr_t)nl){
                    fprintf(f,"XREF %p -> %p : ",(void*)(va+i),(void*)tgt);
                    for(int q=-16;q<48;q++){ if((long long)i+q>=0&&i+q<sz) fprintf(f,"%02X",p[i+q]); }
                    fprintf(f,"\n"); xr++;
                    break;
                }
            }
        }
    }
    fprintf(f,"STRN=%d XREF=%d\nDONE\n",nf,xr); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
