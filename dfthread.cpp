#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <psapi.h>
#pragma comment(lib,"psapi.lib")
typedef LONG NTSTATUS;
typedef struct _OBI { ULONG Attributes; ACCESS_MASK GrantedAccess; ULONG HandleCount, PointerCount;
    ULONG PagedPoolUsage, NonPagedPoolUsage; ULONG Reserved[3]; ULONG NameLen, TypeLen, SdLen; LARGE_INTEGER CreateTime; } OBI;
int wmain(int argc, wchar_t** argv) {
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\thread.log",L"a");
#define L(...) do{ wprintf(__VA_ARGS__); if(f) fwprintf(f,__VA_ARGS__);}while(0)
    DWORD pid=(DWORD)_wtoi(argv[1]);
    auto NtQO=(NTSTATUS(NTAPI*)(HANDLE,ULONG,PVOID,ULONG,PULONG))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQueryObject");
    auto NtCS=(NTSTATUS(NTAPI*)(PHANDLE,ACCESS_MASK,PVOID,PLARGE_INTEGER,ULONG,ULONG,HANDLE))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtCreateSection");
    auto NtMVS=(NTSTATUS(NTAPI*)(HANDLE,HANDLE,PVOID*,ULONG_PTR,SIZE_T,PLARGE_INTEGER,PSIZE_T,ULONG,ULONG,ULONG))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtMapViewOfSection");
    L(L"== pid %lu ==\n",(unsigned long)pid);
    HANDLE h=OpenProcess(PROCESS_ALL_ACCESS,FALSE,pid);
    L(L"OpenProcess ALL h=%p err=%lu\n",h,h?0:GetLastError());
    if(h){ OBI o{}; ULONG r=0; NtQO(h,0,&o,sizeof(o),&r); L(L"proc granted=0x%lX\n",(unsigned long)o.GrantedAccess); }
    HANDLE sec=nullptr; LARGE_INTEGER sz{}; sz.QuadPart=0x1000;
    NTSTATUS st=NtCS(&sec,0x1F0003,nullptr,&sz,PAGE_EXECUTE_READWRITE,SEC_COMMIT,nullptr);
    L(L"NtCreateSection st=0x%08lX sec=%p\n",(unsigned long)st,sec);
    if(sec){
        PVOID base=nullptr; SIZE_T vs=0;
        st=NtMVS(sec,h,&base,0,0,nullptr,&vs,1,0,PAGE_EXECUTE_READWRITE);
        L(L"NtMapViewOfSection st=0x%08lX base=%p vs=%llu\n",(unsigned long)st,base,(unsigned long long)vs);
        if(st==0 && base){ volatile char* p=(char*)base; }
        if(st==0 && base){ }
    }
    // thread rights
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
    if(snap!=INVALID_HANDLE_VALUE){
        THREADENTRY32 te{sizeof(te)}; int n=0;
        if(Thread32First(snap,&te)) do{ if(te.th32OwnerProcessID!=pid) continue;
            HANDLE th=OpenThread(THREAD_ALL_ACCESS,FALSE,te.th32ThreadID);
            if(th){ OBI o{}; ULONG r=0; NtQO(th,0,&o,sizeof(o),&r);
                L(L"tid=%lu granted=0x%lX setctx=%d susp=%d getctx=%d\n",(unsigned long)te.th32ThreadID,(unsigned long)o.GrantedAccess,
                  (o.GrantedAccess&0x10)?1:0,(o.GrantedAccess&0x2)?1:0,(o.GrantedAccess&0x8)?1:0);
                CloseHandle(th); if(++n>=6) break;
            }
        } while(Thread32Next(snap,&te));
        CloseHandle(snap);
    }
    if(h) CloseHandle(h); if(sec) CloseHandle(sec); if(f) fclose(f);
    return 0;
}
