// dfacl2 — ukur granted access handle ke game seiring waktu + uji DuplicateHandle
// Tujuan: temukan jendela "pristine" (di mana VM_READ/VM_OP/CREATE_THREAD masih granted)
// Semua read-only. Print ke stdout (redirect dari luar).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>
#include <string>

typedef LONG NTSTATUS;
typedef struct _OBI { ULONG Attributes; ACCESS_MASK GrantedAccess; ULONG HandleCount, PointerCount;
    ULONG PagedPoolUsage, NonPagedPoolUsage; ULONG Reserved[3]; ULONG NameLen, TypeLen, SdLen; LARGE_INTEGER CreateTime; } OBI;

static const wchar_t* kTarget = L"DeltaForceClient-Win64-Shipping.exe";

static void L(const wchar_t* fmt, ...){
    wchar_t m[2048]; va_list ap; va_start(ap,fmt); wvsprintfW(m,fmt,ap); va_end(ap);
    fwprintf(stdout,L"%s\n",m); fflush(stdout);
}
static DWORD FindPid(){
    HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(s==INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{}; pe.dwSize=sizeof(pe); DWORD pid=0;
    if(Process32FirstW(s,&pe)) do {
        if(_wcsicmp(pe.szExeFile,kTarget)==0){ pid=pe.th32ProcessID; break; }
    } while(Process32NextW(s,&pe));
    CloseHandle(s); return pid;
}

int wmain(int argc, wchar_t** argv){
    int sec = argc>1?_wtoi(argv[1]):300;
    auto NtQO = (NTSTATUS(NTAPI*)(HANDLE,ULONG,PVOID,ULONG,PULONG))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQueryObject");
    auto NtAVM= (NTSTATUS(NTAPI*)(HANDLE,PVOID*,ULONG_PTR,PSIZE_T,ULONG,ULONG))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtAllocateVirtualMemory");
    L(L"ACL2 start durasi=%ds",sec);
    DWORD t0=GetTickCount(); DWORD last=0; int n=0;
    const ACCESS_MASK want = 0x43A;
    while((GetTickCount()-t0)<(DWORD)sec*1000){
        DWORD pid=FindPid();
        if(!pid){ Sleep(200); continue; }
        double el=(GetTickCount()-t0)/1000.0;
        if(pid!=last){ L(L"GAME pid=%lu t+%.2f",(unsigned long)pid,el); last=pid; n=0; }
        n++;
        HANDLE h=OpenProcess(want,FALSE,pid);
        if(!h){ L(L"  t+%.2f #%d OpenProcess FAIL err=%lu",el,n,GetLastError()); Sleep(250); continue; }
        OBI obi{}; ULONG len=0;
        NTSTATUS st=NtQO(h,0,&obi,sizeof(obi),&len);
        ACCESS_MASK g=obi.GrantedAccess;
        // uji nyata: alokasi 4KB
        PVOID a=nullptr; SIZE_T sz=4096;
        NTSTATUS ast=NtAVM(h,&a,0,&sz,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        L(L"  t+%.2f #%d granted=0x%X VM_R=%d VM_W=%d VM_OP=%d THR=%d | NtAlloc=0x%X addr=%p",
          el,n,(unsigned)g,
          (g&0x10)?1:0,(g&0x20)?1:0,(g&0x8)?1:0,(g&0x2)?1:0,(unsigned)ast,a);
        if(a) VirtualFreeEx(h,a,0,MEM_RELEASE);
        CloseHandle(h);
        if(g & 0x28){   // VM_READ|VM_WRITE ada
            L(L"  *** PRISTINE WINDOW t+%.2f granted=0x%X ***",el,(unsigned)g);
        }
        Sleep(250);
    }
    L(L"ACL2 end");
    return 0;
}
