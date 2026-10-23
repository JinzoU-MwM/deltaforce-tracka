#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
typedef LONG NTSTATUS;

static std::vector<BYTE> ReadAll(const wchar_t* p){
    HANDLE f=CreateFileW(p,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(f==INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER sz{}; GetFileSizeEx(f,&sz);
    std::vector<BYTE> b((size_t)sz.QuadPart);
    DWORD rd=0; ReadFile(f,b.data(),(DWORD)b.size(),&rd,nullptr);
    b.resize(rd); CloseHandle(f); return b;
}
static void Logf(const wchar_t* fmt, ...){
    wchar_t msg[2048]; va_list ap; va_start(ap,fmt); wvsprintfW(msg,fmt,ap); va_end(ap);
    wprintf(L"%s\n",msg);
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\secinject.log",L"a");
    if(f){ wchar_t line[2200]; SYSTEMTIME st; GetLocalTime(&st);
        wsprintfW(line,L"[%02d:%02d:%02d] %s\r\n",st.wHour,st.wMinute,st.wSecond,msg);
        char u8[4400]; int n=WideCharToMultiByte(CP_UTF8,0,line,-1,u8,sizeof(u8),nullptr,nullptr);
        if(n>1){ DWORD w; WriteFile(f,u8,(DWORD)(n-1),&w,nullptr); } fclose(f); }
}
int wmain(int argc, wchar_t** argv){
    if(argc<3){ wprintf(L"usage: dfsecinject <pid> <dll>\n"); return 1; }
    DWORD pid=(DWORD)_wtoi(argv[1]); const wchar_t* dll=argv[2];
    auto NtCS=(NTSTATUS(NTAPI*)(PHANDLE,ACCESS_MASK,PVOID,PLARGE_INTEGER,ULONG,ULONG,HANDLE))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtCreateSection");
    auto NtMVS=(NTSTATUS(NTAPI*)(HANDLE,HANDLE,PVOID*,ULONG_PTR,SIZE_T,PLARGE_INTEGER,PSIZE_T,ULONG,ULONG,ULONG))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtMapViewOfSection");
    auto NtQAPC=(NTSTATUS(NTAPI*)(HANDLE,PVOID,PVOID,PVOID,PVOID))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQueueApcThread");
    uintptr_t rtl=(uintptr_t)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlAddFunctionTable");
    std::vector<BYTE> file=ReadAll(dll);
    if(file.size()<0x1000){ Logf(L"read dll failed"); return 2; }
    auto dos=(PIMAGE_DOS_HEADER)file.data();
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE){ Logf(L"bad dos"); return 2; }
    auto nt=(PIMAGE_NT_HEADERS)(file.data()+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE){ Logf(L"bad nt"); return 2; }
    DWORD imgSize=nt->OptionalHeader.SizeOfImage;
    std::vector<BYTE> img(imgSize,0);
    memcpy(img.data(),file.data(),min((size_t)nt->OptionalHeader.SizeOfHeaders,file.size()));
    std::vector<BYTE> fcopy=file;
    auto At=[&](DWORD rva)->BYTE*{
        auto s=IMAGE_FIRST_SECTION(nt);
        for(WORD i=0;i<nt->FileHeader.NumberOfSections;++i,++s){
            DWORD vs=s->Misc.VirtualSize?s->Misc.VirtualSize:s->SizeOfRawData;
            if(rva>=s->VirtualAddress && rva<s->VirtualAddress+vs){
                DWORD off=rva-s->VirtualAddress; if(off>=s->SizeOfRawData) return nullptr;
                return fcopy.data()+s->PointerToRawData+off;
            }
        } return nullptr;
    };
    auto sec=IMAGE_FIRST_SECTION(nt);
    for(WORD i=0;i<nt->FileHeader.NumberOfSections;++i,++sec){
        if(!sec->SizeOfRawData) continue;
        DWORD n=min((size_t)sec->SizeOfRawData,(size_t)sec->Misc.VirtualSize?sec->Misc.VirtualSize:sec->SizeOfRawData);
        if(sec->VirtualAddress+n>imgSize) n=imgSize-sec->VirtualAddress;
        memcpy(img.data()+sec->VirtualAddress,fcopy.data()+sec->PointerToRawData,n);
    }
    SIZE_T imgAlign=((SIZE_T)imgSize+0xFFF)&~(SIZE_T)0xFFF;
    SIZE_T total=imgAlign+0x1000;
    HANDLE secH=nullptr; LARGE_INTEGER sz{}; sz.QuadPart=(LONGLONG)total;
    NTSTATUS st=NtCS(&secH,0xF001F,nullptr,&sz,PAGE_EXECUTE_READWRITE,SEC_COMMIT,nullptr);
    Logf(L"section st=0x%08lX size=%llu",(unsigned long)st,(unsigned long long)total);
    HANDLE h=OpenProcess(0x43A,FALSE,pid);
    Logf(L"open pid=%lu h=%p err=%lu",(unsigned long)pid,h,h?0:GetLastError());
    if(!secH||!h) return 3;
    PVOID gbase=nullptr; SIZE_T gvs=0;
    st=NtMVS(secH,h,&gbase,0,0,nullptr,&gvs,1,0,PAGE_EXECUTE_READWRITE);
    Logf(L"map game st=0x%08lX base=%p vs=%llu",(unsigned long)st,gbase,(unsigned long long)gvs);
    PVOID lbase=nullptr; SIZE_T lvs=0;
    if(st==0) st=NtMVS(secH,GetCurrentProcess(),&lbase,0,0,nullptr,&lvs,1,0,PAGE_EXECUTE_READWRITE);
    Logf(L"map self st=0x%08lX base=%p",(unsigned long)st,lbase);
    if(st!=0||!gbase||!lbase){ if(h)CloseHandle(h); if(secH)CloseHandle(secH); return 4; }
    uintptr_t delta=(uintptr_t)gbase-(uintptr_t)nt->OptionalHeader.ImageBase;
    auto& rd=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if(delta && rd.Size){
        BYTE* rp=At(rd.VirtualAddress); BYTE* rend=rp?rp+rd.Size:nullptr;
        while(rp&&rend&&rp+sizeof(IMAGE_BASE_RELOCATION)<=rend){
            auto blk=(PIMAGE_BASE_RELOCATION)rp;
            if(!blk->SizeOfBlock||blk->SizeOfBlock<sizeof(IMAGE_BASE_RELOCATION)) break;
            DWORD n=(blk->SizeOfBlock-sizeof(IMAGE_BASE_RELOCATION))/sizeof(WORD);
            WORD* e=(WORD*)(blk+1);
            for(DWORD i=0;i<n;++i){
                int type=e[i]>>12, off=e[i]&0xFFF; BYTE* p=img.data()+blk->VirtualAddress+off;
                if(type==IMAGE_REL_BASED_DIR64 && (BYTE*)(img.data()+imgSize)>=p+8) *(uint64_t*)p += delta;
                else if(type==IMAGE_REL_BASED_HIGHLOW && (BYTE*)(img.data()+imgSize)>=p+4) *(uint32_t*)p += (uint32_t)delta;
            }
            rp+=blk->SizeOfBlock;
        }
    }
    auto& imp=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if(imp.Size){
        BYTE* dp=At(imp.VirtualAddress); BYTE* dend=dp?dp+imp.Size:nullptr;
        while(dp&&dend&&dp+sizeof(IMAGE_IMPORT_DESCRIPTOR)<=dend){
            auto d=(PIMAGE_IMPORT_DESCRIPTOR)dp;
            if(!d->Name) break;
            const char* dllName=(const char*)At(d->Name);
            HMODULE mod=dllName?GetModuleHandleA(dllName):nullptr;
            if(!mod&&dllName) mod=LoadLibraryA(dllName);
            if(!mod){ Logf(L"import dll fail"); return 5; }
            BYTE* tp=At(d->OriginalFirstThunk?d->OriginalFirstThunk:d->FirstThunk);
            DWORD idx=0;
            while(tp){
                auto th=(PIMAGE_THUNK_DATA64)(tp+(SIZE_T)idx*8);
                if(!th->u1.AddressOfData) break;
                uintptr_t fn=0;
                if(th->u1.Ordinal&IMAGE_ORDINAL_FLAG64) fn=(uintptr_t)GetProcAddress(mod,(LPCSTR)(th->u1.Ordinal&0xFFFF));
                else { auto ibn=(PIMAGE_IMPORT_BY_NAME)At((DWORD)th->u1.AddressOfData); if(!ibn) return 5; fn=(uintptr_t)GetProcAddress(mod,(LPCSTR)ibn->Name); }
                if(!fn){ Logf(L"import fn fail"); return 5; }
                *(uint64_t*)(img.data()+d->FirstThunk+(SIZE_T)idx*8)=fn;
                ++idx;
            }
            dp+=sizeof(IMAGE_IMPORT_DESCRIPTOR);
        }
    }
    memcpy(lbase,img.data(),imgSize);
    SIZE_T stubOff=imgAlign, guardOff=stubOff+0x700;
    uint64_t stubAddr=(uint64_t)(uintptr_t)gbase+stubOff;
    uint64_t guardAddr=(uint64_t)(uintptr_t)gbase+guardOff;
    uint64_t statusAddr=guardAddr+4;
    auto& exd=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    uint64_t tableAddr=exd.Size?(uint64_t)(uintptr_t)gbase+exd.VirtualAddress:0;
    DWORD tableCount=exd.Size/12;
    std::vector<BYTE> c;
    auto b=[&](std::initializer_list<BYTE> v){ c.insert(c.end(),v); };
    auto i32=[&](uint32_t v){ BYTE t[4]; memcpy(t,&v,4); c.insert(c.end(),t,t+4); };
    auto i64=[&](uint64_t v){ BYTE t[8]; memcpy(t,&v,8); c.insert(c.end(),t,t+8); };
    // guard
    b({0xB8}); i32(1);                       // mov eax,1
    size_t xchgDisp=c.size()+2; b({0x87,0x05}); i32(0); // xchg [rip+disp],eax
    b({0x85,0xC0});                          // test eax,eax
    size_t jnzPos=c.size()+1; b({0x75,0x00});// jnz ret
    // status=1
    size_t s1=c.size()+2; b({0xC7,0x05}); i32(0); i32(1);
    // rtl
    b({0x48,0xB9}); i64(tableAddr);
    b({0xBA}); i32(tableCount);
    b({0x49,0xB8}); i64((uint64_t)(uintptr_t)gbase);
    b({0x48,0xB8}); i64(rtl);
    b({0x48,0x83,0xEC,0x28}); b({0xFF,0xD0}); b({0x48,0x83,0xC4,0x28});
    // status=2
    size_t s2=c.size()+2; b({0xC7,0x05}); i32(0); i32(2);
    // entry
    b({0x48,0xB9}); i64((uint64_t)(uintptr_t)gbase);
    b({0xBA}); i32(1);
    b({0x45,0x31,0xC0});
    b({0x48,0xB8}); i64((uint64_t)(uintptr_t)gbase+nt->OptionalHeader.AddressOfEntryPoint);
    b({0x48,0x83,0xEC,0x28}); b({0xFF,0xD0}); b({0x48,0x83,0xC4,0x28});
    // status=3
    size_t s3=c.size()+2; b({0xC7,0x05}); i32(0); i32(3);
    size_t retPos=c.size(); b({0xC3});
    auto putDisp=[&](size_t dispPos,size_t nextPos,uint64_t target){
        int64_t d=(int64_t)target-(int64_t)(stubAddr+nextPos); *(int32_t*)&c[dispPos]=(int32_t)d;
    };
    putDisp(xchgDisp,xchgDisp+4,guardAddr);
    putDisp(s1,s1+4,statusAddr);
    putDisp(s2,s2+4,statusAddr);
    putDisp(s3,s3+4,statusAddr);
    c[jnzPos]=(BYTE)(retPos-(jnzPos+1));
    memcpy((BYTE*)lbase+stubOff,c.data(),c.size());
    *(volatile DWORD*)((BYTE*)lbase+guardOff)=0;
    Logf(L"stub off=%llu len=%llu entryRva=0x%lX table=%llu count=%lu",
         (unsigned long long)stubOff,(unsigned long long)c.size(),
         (unsigned long)nt->OptionalHeader.AddressOfEntryPoint,(unsigned long long)tableCount,(unsigned long)tableCount);
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
    int queued=0, threads=0;
    if(snap!=INVALID_HANDLE_VALUE){
        THREADENTRY32 te{sizeof(te)};
        if(Thread32First(snap,&te)) do{
            if(te.th32OwnerProcessID!=pid) continue;
            HANDLE th=OpenThread(THREAD_ALL_ACCESS,FALSE,te.th32ThreadID);
            if(!th) continue;
            NTSTATUS q=NtQAPC(th,(PVOID)stubAddr,nullptr,nullptr,nullptr);
            if(q==0) queued++;
            CloseHandle(th); threads++;
        }while(Thread32Next(snap,&te));
        CloseHandle(snap);
    }
    Logf(L"apc threads=%d queued=%d",threads,queued);
    DWORD status=0;
    for(int i=0;i<40;++i){ Sleep(100); status=*(volatile DWORD*)((BYTE*)lbase+guardOff); if(status>=3) break; }
    Logf(L"loader_status=%lu",(unsigned long)status);
    if(h)CloseHandle(h); if(secH)CloseHandle(secH);
    return status==3?0:6;
}
