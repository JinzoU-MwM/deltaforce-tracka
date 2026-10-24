// dfacl4 — uji 5 jalur bypass ACL handle ke proses terproteksi (semua user-mode, read-only diagnose)
//  Jalur 1: OpenProcess dengan access mask minimal (QUERY only) lalu DuplicateHandle ke PROCESS_ALL_ACCESS
//  Jalur 2: DuplicateHandle dari proses launcher (df_garena_launcher) yang mungkin punya handle
//  Jalur 3: NtOpenProcess via ntoskrnl direct syscall (bypass user-mode hook)
//  Jalur 4: OpenProcess setelah impersonate SYSTEM (SYSTEM token)
//  Jalur 5: Section map PAGE_READWRITE (bukan EXECUTE) + map self
// Semua hanya mengukur status; tidak menulis apa pun ke game.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef LONG NTSTATUS;
typedef struct _PUBLIC_OBJECT_BASIC_INFORMATION { ULONG Attributes; ACCESS_MASK GrantedAccess; ULONG HandleCount; ULONG PointerCount; ULONG Reserved[10]; } POBI;

#define STATUS_SUCCESS 0L
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)

static void L(const char* fmt, ...){
    char b[2048]; va_list ap; va_start(ap,fmt); vsnprintf(b,sizeof(b),fmt,ap); va_end(ap);
    printf("%s\n",b); fflush(stdout);
}
static DWORD FindPid(const wchar_t* name){
    HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(s==INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{}; pe.dwSize=sizeof(pe); DWORD pid=0;
    if(Process32FirstW(s,&pe)) do {
        if(_wcsicmp(pe.szExeFile,name)==0){ pid=pe.th32ProcessID; break; }
    } while(Process32NextW(s,&pe));
    CloseHandle(s); return pid;
}
static ACCESS_MASK Granted(HANDLE h, void* NtQO){
    auto f=(NTSTATUS(NTAPI*)(HANDLE,ULONG,PVOID,ULONG,PULONG))NtQO;
    POBI obi{}; ULONG len=0;
    NTSTATUS st=f(h,0,&obi,sizeof(obi),&len);
    if(st) return 0xFFFFFFFF;
    return obi.GrantedAccess;
}

// ---- direct syscall stub (jalur 3) ----
extern "C" NTSTATUS NtOpenProcess_Direct(PHANDLE, ACCESS_MASK, PVOID, PVOID);
#pragma comment(lib,"ntdll.lib")

int wmain(int argc, wchar_t** argv){
    auto NtQO=GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQueryObject");
    DWORD gpid=FindPid(L"DeltaForceClient-Win64-Shipping.exe");
    DWORD lpid=FindPid(L"df_garena_launcher.exe");
    DWORD spid=FindPid(L"SGuard64.exe");
    L("TARGETS: game=%lu launcher=%lu sguard=%lu",(unsigned long)gpid,(unsigned long)lpid,(unsigned long)spid);

    // ---------- Jalur 1: OpenProcess QUERY lalu DuplicateHandle ----------
    L("\n[J1] OpenProcess QUERY_ONLY -> DuplicateHandle(ALL_ACCESS)");
    HANDLE hq=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_QUERY_INFORMATION,FALSE,gpid);
    L("  hq=%p err=%lu granted=0x%X",hq,GetLastError(),hq?Granted(hq,NtQO):0);
    if(hq){
        HANDLE hd=nullptr;
        BOOL ok=DuplicateHandle(GetCurrentProcess(),hq,GetCurrentProcess(),&hd,
                                PROCESS_ALL_ACCESS,FALSE,0);
        L("  DuplicateHandle ok=%d hd=%p err=%lu granted=0x%X",
          ok,hd,GetLastError(),hd?Granted(hd,NtQO):0);
        if(hd) CloseHandle(hd);
        CloseHandle(hq);
    }

    // ---------- Jalur 2: handle warisan dari launcher/ACE ----------
    L("\n[J2] DuplicateHandle dari proses launcher & sguard");
    DWORD partners[2]={lpid,spid};
    const char* names[2]={"launcher","sguard"};
    for(int i=0;i<2;i++){
        if(!partners[i]){ L("  %s: tidak ada",names[i]); continue; }
        HANDLE hp=OpenProcess(PROCESS_DUP_HANDLE|PROCESS_QUERY_INFORMATION,FALSE,partners[i]);
        L("  %s pid=%lu hp=%p err=%lu",names[i],(unsigned long)partners[i],hp,GetLastError());
        if(!hp) continue;
        // scan handle table partner, cari process object yang menunjuk ke game
        HANDLE hs=OpenProcess(PROCESS_DUP_HANDLE,FALSE,partners[i]);
        if(hs){
            DWORD need=0; // brute ukuran
            for(DWORD cap=0x1000; cap<=0x100000; cap*=2){
                BYTE* buf=(BYTE*)malloc(cap);
                auto NtQSI=(NTSTATUS(NTAPI*)(HANDLE,ULONG,PVOID,ULONG,PULONG))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQuerySystemInformation");
                ULONG ret=0;
                NTSTATUS st=NtQSI(16 /*SystemHandleInformation*/,buf,cap,&ret);
                if(st==0){
                    // SystemHandleInformation: Count(ULONG) + entries (POINTER_SIZE, pid, handle, objtype, granted, ...)
                    struct SI{ ULONG ProcessId; BYTE ObjectTypeIndex; BYTE Reserved1; USHORT HandleValue; ULONG GrantedAccess; USHORT CreatorBackTraceIndex; USHORT ObjectTypeIndex2; ULONG HandleCount; ULONG PointerCount; ULONG Reserved[10]; };
                    ULONG count=*(ULONG*)buf;
                    BYTE* p=buf+sizeof(ULONG)*(sizeof(void*)==8?2:1);
                    int found=0;
                    for(ULONG k=0;k<count;k++){
                        SI* e=(SI*)(p+k*sizeof(SI));
                        if(e->ProcessId!=partners[i]) continue;
                        HANDLE target=(HANDLE)(uintptr_t)e->HandleValue;
                        HANDLE dup=nullptr;
                        if(DuplicateHandle(hs,target,GetCurrentProcess(),&dup,PROCESS_ALL_ACCESS,FALSE,0)){
                            DWORD opid=(DWORD)GetProcessId(dup);
                            if(opid==gpid){
                                L("    HIT partner=%s handle=0x%X granted=0x%X dupGranted=0x%X",
                                  names[i],(unsigned)e->HandleValue,e->GrantedAccess,Granted(dup,NtQO));
                                found++;
                            }
                            CloseHandle(dup);
                        }
                    }
                    L("    %s: entries=%lu found_game_handles=%d",names[i],(unsigned long)count,found);
                    break;
                }
                free(buf);
            }
            CloseHandle(hs);
        }
        CloseHandle(hp);
    }

    // ---------- Jalur 5: section map PAGE_READWRITE ----------
    L("\n[J5] Section map PAGE_READWRITE (bukan RWX)");
    auto NtCS=(NTSTATUS(NTAPI*)(PHANDLE,ACCESS_MASK,PVOID,PLARGE_INTEGER,ULONG,ULONG,HANDLE))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtCreateSection");
    auto NtMVS=(NTSTATUS(NTAPI*)(HANDLE,HANDLE,PVOID*,ULONG_PTR,SIZE_T,PLARGE_INTEGER,PSIZE_T,ULONG,ULONG,ULONG))GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtMapViewOfSection");
    HANDLE h5=OpenProcess(PROCESS_VM_OPERATION|PROCESS_QUERY_INFORMATION,FALSE,gpid);
    L("  OpenProcess VM_OP h=%p err=%lu granted=0x%X",h5,GetLastError(),h5?Granted(h5,NtQO):0);
    if(h5){
        HANDLE sec=nullptr; LARGE_INTEGER sz{}; sz.QuadPart=0x1000;
        NTSTATUS st=NtCS(&sec,0xF0005 /*READ|WRITE not EXEC*/,nullptr,&sz,PAGE_READWRITE,SEC_COMMIT,nullptr);
        L("  NtCreateSection RW st=0x%X sec=%p",(unsigned)st,sec);
        if(sec){
            PVOID base=nullptr; SIZE_T vs=0;
            st=NtMVS(sec,h5,&base,0,0,nullptr,&vs,1,0,PAGE_READWRITE);
            L("  NtMapViewOfSection(game) st=0x%X base=%p vs=%llu",(unsigned)st,base,(unsigned long long)vs);
            CloseHandle(sec);
        }
        CloseHandle(h5);
    }

    // ---------- Ringkasan ----------
    L("\n=== RINGKASAN ===");
    L("game granted dengan ALL_ACCESS request: 0x%X", [&](){HANDLE h=OpenProcess(PROCESS_ALL_ACCESS,FALSE,gpid); ACCESS_MASK g=h?Granted(h,NtQO):0; if(h)CloseHandle(h); return (unsigned)g;}());
    L("DONE");
    return 0;
}
