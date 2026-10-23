#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include "map.h"
static LONG WINAPI OnCrash(EXCEPTION_POINTERS* e) {
    wprintf(L"CRASH step=%ld code=%08lX addr=%p\n", g_mapStep, e->ExceptionRecord->ExceptionCode, e->ExceptionRecord->ExceptionAddress);
    fflush(stdout);
    return EXCEPTION_EXECUTE_HANDLER;
}

static DWORD FindPid(const wchar_t* name) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{ sizeof(pe) }; DWORD pid = 0;
    if (Process32FirstW(s, &pe)) do { if (_wcsicmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; } } while (Process32NextW(s, &pe));
    CloseHandle(s); return pid;
}
int wmain(int argc, wchar_t** argv) {
    if (argc < 3) { wprintf(L"usage: dfmap.exe <pid|name> <dll>\n"); return 1; }
    DWORD pid = iswdigit(argv[1][0]) ? (DWORD)_wtoi(argv[1]) : FindPid(argv[1]);
    if (!pid) { wprintf(L"no pid\n"); return 2; }
    HANDLE h = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                           PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!h) { wprintf(L"OpenProcess %lu\n", GetLastError()); return 3; }
    SetUnhandledExceptionFilter(OnCrash);
    DWORD err = 0;
    bool ok = ManualMap(h, argv[2], &err);
    wprintf(L"manualmap pid=%lu ok=%d err=%lu\n", pid, ok ? 1 : 0, err);
    CloseHandle(h);
    return ok ? 0 : 4;
}
