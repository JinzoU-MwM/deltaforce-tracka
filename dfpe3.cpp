#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
static bool Readable(uintptr_t a,size_t n){ if(!a)return false; MEMORY_BASIC_INFORMATION m{}; if(!VirtualQuery((LPCVOID)a,&m,sizeof(m)))return false; return m.State==MEM_COMMIT&&!(m.Protect&PAGE_NOACCESS)&&(uintptr_t)m.BaseAddress+m.RegionSize>=a+n; }
static DWORD WINAPI Th(LPVOID){
    uintptr_t B=(uintptr_t)GetModuleHandleW(nullptr);
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\pe3.txt",L"w"); if(!f) return 1;
    auto dos=(PIMAGE_DOS_HEADER)B; auto nt=(PIMAGE_NT_HEADERS)(B+dos->e_lfanew); auto sec=IMAGE_FIRST_SECTION(nt);
    int hits=0;
    for(WORD si=0;si<nt->FileHeader.NumberOfSections&&hits<120;++si,++sec){
        if(!(sec->Characteristics&IMAGE_SCN_MEM_EXECUTE)) continue;
        uintptr_t va=B+sec->VirtualAddress; size_t sz=sec->Misc.VirtualSize?sec->Misc.VirtualSize:sec->SizeOfRawData;
        if(!Readable(va,sz)) continue; const unsigned char* p=(const unsigned char*)va;
        for(size_t i=2;i+6<sz;i++){
            if(p[i]!=0xD0||p[i+1]!=0x01||p[i+2]!=0x00||p[i+3]!=0x00) continue;
            // preceding: 48 8B modrm  (mov r64, [reg+disp32]) with mod=10
            if(!(p[i-2]==0x48||p[i-2]==0x4C)||p[i-1]!=0x8B) continue;
            unsigned char modrm=p[i-3]; (void)modrm;
            // bytes: 48 8B <modrm> D0 01 00 00 ; modrm at i-1? Actually p[i-2]=48,p[i-1]=8B,p[i]=D0 -> modrm is p[i]? no.
            continue;
        }
        // correct pattern: 48 8B <modrm> D0 01 00 00  -> D0 is disp low; modrm at i-1 where p[i-1]=D0? 
        for(size_t i=0;i+7<sz;i++){
            if(p[i]==0x48&&p[i+1]==0x8B){
                unsigned char m=p[i+2];
                if((m&0xC7)==0x82 || (m&0xC7)==0x8A || (m&0xC7)==0x92 || (m&0xC7)==0x9A ||
                   (m&0xC7)==0x83 || (m&0xC7)==0x8B || (m&0xC7)==0x93 || (m&0xC7)==0x9B){
                    if(p[i+3]==0xD0&&p[i+4]==0x01&&p[i+5]==0x00&&p[i+6]==0x00){
                        uintptr_t a=va+i;
                        fprintf(f,"MOV %p bytes: ",(void*)a);
                        for(int k=-24;k<48;k++){ if(a+k>=va&&a+k<va+sz) fprintf(f,"%02X",p[i+k]); }
                        fprintf(f,"\n");
                        if(++hits>=120) break;
                    }
                }
            }
        }
        if(hits>=120) break;
    }
    fprintf(f,"HITS=%d\nDONE\n",hits); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
