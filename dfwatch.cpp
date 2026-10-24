// dfwatch — watch proses game; inject sedini mungkin, retry berkala sampai sukses
// Dijalankan elevated. Log ke watch.log. Inject ke dfenc.dll (read-only probe)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <vector>
#include <string>

static const wchar_t* kTarget = L"DeltaForceClient-Win64-Shipping.exe";
static const wchar_t* kDll = L"C:\\Users\\User\\trackA\\dfenc.dll";
static const wchar_t* kInjector = L"C:\\Users\\User\\trackA\\dfsecinject.exe";

static void Log(const wchar_t* fmt, ...){
    wchar_t msg[2048]; va_list ap; va_start(ap,fmt); wvsprintfW(msg,fmt,ap); va_end(ap);
    wprintf(L"%s\n",msg);
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\watch.log",L"a");
    if(f){ SYSTEMTIME st; GetLocalTime(&st);
        char u8[4400];
        wchar_t line[2600];
        wsprintfW(line,L"[%02d:%02d:%02d.%03d] %s\r\n",st.wHour,st.wMinute,st.wSecond,st.wMilliseconds,msg);
        int n=WideCharToMultiByte(CP_UTF8,0,line,-1,u8,sizeof(u8),nullptr,nullptr);
        if(n>1){ DWORD w; WriteFile(f,u8,(DWORD)(n-1),&w,nullptr); }
        fclose(f); }
}

static DWORD FindPid(){
    HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(s==INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{}; pe.dwSize=sizeof(pe);
    DWORD pid=0;
    if(Process32FirstW(s,&pe)){
        do {
            if(_wcsicmp(pe.szExeFile,kTarget)==0){ pid=pe.th32ProcessID; break; }
        } while(Process32NextW(s,&pe));
    }
    CloseHandle(s); return pid;
}

// coba inject, kembalikan true kalau proses injector exit 0
static bool TryInject(DWORD pid, int* outExit){
    wchar_t cmd[1024];
    wsprintfW(cmd,L"\"%s\" %lu \"%s\"",kInjector,(unsigned long)pid,kDll);
    wchar_t logPath[MAX_PATH];
    wsprintfW(logPath,L"C:\\Users\\User\\trackA\\inj_%lu.log",(unsigned long)pid);
    // redirect lewat cmd
    wchar_t full[1600];
    wsprintfW(full,L"cmd.exe /c %s > \"%s\" 2>&1",cmd,logPath);

    STARTUPINFOW si{}; si.cb=sizeof(si);
    si.dwFlags=STARTF_USESHOWWINDOW; si.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION pi{};
    std::wstring line=full;
    if(!CreateProcessW(nullptr,&line[0],nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)){
        Log(L"CreateProcess injector failed err=%lu",GetLastError()); return false;
    }
    WaitForSingleObject(pi.hProcess,10000);
    DWORD ec=999; GetExitCodeProcess(pi.hProcess,&ec);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    if(outExit) *outExit=(int)ec;
    return ec==0;
}

int wmain(int argc, wchar_t** argv){
    int totalSec = argc>1 ? _wtoi(argv[1]) : 300;   // durasi watch default 5 menit
    if(argc>2) kDll = argv[2];
    DeleteFileW(L"C:\\Users\\User\\trackA\\watch.log");
    Log(L"watch start target=%s dll=%s durasi=%ds",kTarget,kDll,totalSec);

    DWORD lastPid=0; int attempt=0; int okCount=0;
    DWORD t0=GetTickCount();
    while((GetTickCount()-t0) < (DWORD)totalSec*1000){
        DWORD pid=FindPid();
        if(pid && pid!=lastPid){
            Log(L"GAME FOUND pid=%lu (t+%.1fs)",(unsigned long)pid,(GetTickCount()-t0)/1000.0);
            lastPid=pid; attempt=0;
        }
        if(pid){
            attempt++;
            int ec=-1;
            bool ok=TryInject(pid,&ec);
            Log(L"  attempt #%d pid=%lu exit=%d %s",attempt,(unsigned long)pid,ec,ok?L"OK":L"FAIL");
            if(ok){
                okCount++;
                Log(L"  inject OK #%d — tunggu dump",okCount);
                Sleep(6000);
                // cek file hasil
                if(GetFileAttributesW(L"C:\\Users\\User\\trackA\\enc.txt")!=INVALID_FILE_ATTRIBUTES){
                    Log(L"  enc.txt ADA — probe jalan"); 
                }
                // kalau sudah sukses 1x dan file ada, tetap lanjut watch (game bisa restart)
                if(okCount>=1 && GetFileAttributesW(L"C:\\Users\\User\\trackA\\enc.txt")!=INVALID_FILE_ATTRIBUTES){
                    Log(L"  target tercapai, stop watch");
                    break;
                }
            }
            Sleep(attempt<10 ? 2000 : 15000);
        } else {
            Sleep(500);
        }
    }
    Log(L"watch end ok=%d",okCount);
    return 0;
}
