#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
static bool Readable(uintptr_t a,size_t n){ if(!a)return false; MEMORY_BASIC_INFORMATION m{}; if(!VirtualQuery((LPCVOID)a,&m,sizeof(m)))return false; return m.State==MEM_COMMIT&&!(m.Protect&PAGE_NOACCESS)&&(uintptr_t)m.BaseAddress+m.RegionSize>=a+n; }
static DWORD WINAPI Th(LPVOID){
    uintptr_t B=(uintptr_t)GetModuleHandleW(nullptr);
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\scan2.txt",L"w"); if(!f)return 1;
    fprintf(f,"BASE=%p\n",(void*)B);
    auto dos=(PIMAGE_DOS_HEADER)B; auto nt=(PIMAGE_NT_HEADERS)(B+dos->e_lfanew); auto sec=IMAGE_FIRST_SECTION(nt);
    int hits=0;
    for(WORD si=0; si<nt->FileHeader.NumberOfSections && hits<500; ++si,++sec){
        if(!(sec->Characteristics&IMAGE_SCN_MEM_EXECUTE)) continue;
        uintptr_t va=B+sec->VirtualAddress; size_t sz=sec->Misc.VirtualSize?sec->Misc.VirtualSize:sec->SizeOfRawData;
        if(!Readable(va,sz)) continue;
        const BYTE* p=(const BYTE*)va;
        for(size_t o=0;o+12<sz;++o){
            if(p[o]!=0x48 && p[o]!=0x49) continue;
            BYTE op=p[o+1];
            if(op<0xB8||op>0xBF) continue;
            uint64_t imm=*(uint64_t*)(p+o+2);
            // MAGIC 0x00004A0000000000 -> imm bytes [5]=0x4A [6]=0x00 [7]=0x00
            if(((imm>>40)&0xFF)!=0x4A) continue;
            if(((imm>>48)&0xFF)!=0x00) continue;
            if(((imm>>56)&0xFF)!=0x00) continue;
            fprintf(f,"HIT %p imm=%llX op=%02X%02X\n",(void*)(va+o),(unsigned long long)imm,p[o],op);
            size_t st=(o>64)?o-64:0;
            for(size_t k=st;k<o+128&&k<sz;++k) fprintf(f,"%02X",p[k]);
            fprintf(f,"\n");
            if(++hits>=500) break;
        }
    }
    fprintf(f,"HITS=%d\n",hits); fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
