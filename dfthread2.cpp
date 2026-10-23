#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
typedef LONG NTSTATUS;
int wmain(int argc, wchar_t** argv){
 FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\thread2.log",L"a");
#define L(...) do{ wprintf(__VA_ARGS__); if(f) fwprintf(f,__VA_ARGS__);}while(0)
 DWORD pid=(DWORD)_wtoi(argv[1]);
 auto NtCS=(NTSTATUS(NTAPI*)(PHANDLE,ACCESS_MASK,PVOID,PLARGE_INTEGER,ULONG,ULONG,HANDLE))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtCreateSection");
 auto NtMVS=(NTSTATUS(NTAPI*)(HANDLE,HANDLE,PVOID*,ULONG_PTR,SIZE_T,PLARGE_INTEGER,PSIZE_T,ULONG,ULONG,ULONG))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtMapViewOfSection");
 auto NtGCT=(NTSTATUS(NTAPI*)(HANDLE,PCONTEXT))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtGetContextThread");
 auto NtSCT=(NTSTATUS(NTAPI*)(HANDLE,PCONTEXT))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtSetContextThread");
 auto NtST=(NTSTATUS(NTAPI*)(HANDLE))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtSuspendThread");
 HANDLE h=OpenProcess(0x43A,FALSE,pid);
 L(L"open=%p err=%lu\n",h,h?0:GetLastError());
 HANDLE sec=nullptr; LARGE_INTEGER sz{}; sz.QuadPart=0x2000;
 NTSTATUS st=NtCS(&sec,0xF001F,nullptr,&sz,PAGE_EXECUTE_READWRITE,SEC_COMMIT,nullptr);
 L(L"section st=0x%08lX h=%p\n",(unsigned long)st,sec);
 if(sec&&h){
   for(int prot=0;prot<2;++prot){
     PVOID base=nullptr; SIZE_T vs=0; ULONG p = prot? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
     st=NtMVS(sec,h,&base,0,0,nullptr,&vs,1,0,p);
     L(L"map prot=0x%lX st=0x%08lX base=%p vs=%llu\n",(unsigned long)p,(unsigned long)st,base,(unsigned long long)vs);
   }
 }
 if(h){
   HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
   if(snap!=INVALID_HANDLE_VALUE){ THREADENTRY32 te{sizeof(te)}; int n=0;
     if(Thread32First(snap,&te)) do{ if(te.th32OwnerProcessID!=pid) continue;
       HANDLE th=OpenThread(THREAD_ALL_ACCESS,FALSE,te.th32ThreadID);
       if(!th) continue;
       CONTEXT c{}; c.ContextFlags=CONTEXT_FULL;
       NTSTATUS g=NtGCT(th,&c);
       NTSTATUS s2 = (g==0)? NtSCT(th,&c) : -1;
       NTSTATUS su=NtST(th);
       L(L"tid=%lu get=0x%08lX set=0x%08lX susp=0x%08lX rip=%p\n",(unsigned long)te.th32ThreadID,(unsigned long)g,(unsigned long)s2,(unsigned long)su,(void*)c.Rip);
       CloseHandle(th); if(++n>=4) break;
     }while(Thread32Next(snap,&te));
     CloseHandle(snap);
   }
 }
 if(h)CloseHandle(h); if(sec)CloseHandle(sec); if(f)fclose(f); return 0;
}
