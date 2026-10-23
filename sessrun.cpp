#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wtsapi32.h>
#include <stdio.h>
#pragma comment(lib,"wtsapi32.lib")
static void En(HANDLE t, const wchar_t* n){ TOKEN_PRIVILEGES tp{}; LUID l; if(!LookupPrivilegeValueW(nullptr,n,&l))return; tp.PrivilegeCount=1; tp.Privileges[0].Luid=l; tp.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED; AdjustTokenPrivileges(t,FALSE,&tp,sizeof(tp),nullptr,nullptr); }
int wmain(int argc, wchar_t** argv){
    DWORD sid=WTSGetActiveConsoleSessionId();
    HANDLE tok=nullptr;
    if(!WTSQueryUserToken(sid,&tok)){ wprintf(L"WTSQueryUserToken %lu\n",GetLastError()); return 1; }
    En(tok,L"SeAssignPrimaryTokenPrivilege"); En(tok,L"SeIncreaseQuotaPrivilege");
    STARTUPINFOW si{sizeof(si)}; si.lpDesktop=L"winsta0\\default";
    PROCESS_INFORMATION pi{};
    const wchar_t* exe = (argc > 1) ? argv[1] : L"C:\\Users\\User\\trackA\\dfoverlay.exe";
    wchar_t cmd[1024]; wsprintfW(cmd, L"\"%s\"", exe);
    BOOL ok=CreateProcessAsUserW(tok,exe,cmd,nullptr,nullptr,FALSE,0,nullptr,L"C:\\Users\\User\\trackA",&si,&pi);
    wprintf(L"session=%lu ok=%d err=%lu pid=%lu\n",sid,ok,ok?0:GetLastError(),ok?pi.dwProcessId:0);
    if(ok){ WaitForSingleObject(pi.hProcess,3000); DWORD ec=0xFFFFFFFF; GetExitCodeProcess(pi.hProcess,&ec); wprintf(L"exit=%lu\n",ec); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    CloseHandle(tok); return ok?0:2;
}
