#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include "map.h"
#include <stdarg.h>
#include <stdio.h>

static DWORD FindPid(const wchar_t* name) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{ sizeof(pe) }; DWORD pid = 0;
    if (Process32FirstW(s, &pe)) do {
        if (_wcsicmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
    } while (Process32NextW(s, &pe));
    CloseHandle(s);
    return pid;
}
static void StripFile(wchar_t* p) {
    for (int i = (int)wcslen(p) - 1; i >= 0; --i) if (p[i] == L'\\' || p[i] == L'/') { p[i] = 0; break; }
}
static void Logf(const wchar_t* fmt, ...) {
    wchar_t dir[MAX_PATH]; GetModuleFileNameW(nullptr, dir, MAX_PATH); StripFile(dir);
    wchar_t path[MAX_PATH]; wsprintfW(path, L"%s\\inject.log", dir);
    wchar_t msg[2048]; va_list ap; va_start(ap, fmt); wvsprintfW(msg, fmt, ap); va_end(ap);
    SYSTEMTIME st; GetLocalTime(&st);
    wchar_t line[2200]; wsprintfW(line, L"[%02d:%02d:%02d] %s\r\n", st.wHour, st.wMinute, st.wSecond, msg);
    HANDLE f = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        char u8[4400]; int n = WideCharToMultiByte(CP_UTF8, 0, line, -1, u8, (int)sizeof(u8), nullptr, nullptr);
        DWORD wr = 0; if (n > 1) WriteFile(f, u8, (DWORD)(n - 1), &wr, nullptr);
        CloseHandle(f);
    }
}

static bool EnableDebugPriv() {
    HANDLE tok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok)) return false;
    LUID luid{};
    if (!LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &luid)) { CloseHandle(tok); return false; }
    TOKEN_PRIVILEGES tp{}; tp.PrivilegeCount = 1; tp.Privileges[0].Luid = luid; tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    BOOL ok = AdjustTokenPrivileges(tok, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    DWORD e = GetLastError();
    CloseHandle(tok);
    SetLastError(e);
    return ok && e == ERROR_SUCCESS;
}

static bool Inject(DWORD pid, const wchar_t* dll, DWORD* err);
static bool Inject(DWORD pid, const wchar_t* dll, DWORD* err) {
    HANDLE h = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                           PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!h) { *err = GetLastError(); return false; }
    SIZE_T n = (wcslen(dll) + 1) * sizeof(wchar_t);
    void* r = VirtualAllocEx(h, nullptr, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!r) { *err = GetLastError(); CloseHandle(h); return false; }
    if (!WriteProcessMemory(h, r, dll, n, nullptr)) { *err = GetLastError(); CloseHandle(h); return false; }
    void* load = (void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HANDLE t = CreateRemoteThread(h, nullptr, 0, (LPTHREAD_START_ROUTINE)load, r, 0, nullptr);
    if (!t) { *err = GetLastError(); CloseHandle(h); return false; }
    WaitForSingleObject(t, 10000);
    DWORD ec = 0; GetExitCodeThread(t, &ec);
    CloseHandle(t); CloseHandle(h);
    *err = ec;                 // non-zero => module base
    return ec != 0;
}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR cmd, int) {
    wchar_t dir[MAX_PATH]; GetModuleFileNameW(nullptr, dir, MAX_PATH); StripFile(dir);
    wchar_t core[MAX_PATH], ovl[MAX_PATH];
    wsprintfW(core, L"%s\\dfcore.dll", dir);
    wsprintfW(ovl, L"%s\\dfoverlay.exe", dir);

    if (wcsstr(cmd, L"--test") && wcsstr(cmd, L"--pid")) {
        DWORD pid = 0;
        const wchar_t* p = wcsstr(cmd, L"--pid") + 5;
        pid = (DWORD)_wtoi(p);
        DWORD err = 0;
        HANDLE hp = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                                PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
        bool ok = hp && ManualMap(hp, core, &err);
        if (hp) CloseHandle(hp);
        wprintf(L"test pid=%lu ok=%d code=%lu\n", pid, ok ? 1 : 0, err);
        return ok ? 0 : 1;
    }

    bool dbg = EnableDebugPriv();
    DWORD dbgErr = dbg ? 0 : GetLastError();
    Logf(L"start elevated_priv=%d err=%lu", dbg ? 1 : 0, dbgErr);
    DWORD pid = FindPid(L"DeltaForceClient-Win64-Shipping.exe");
    Logf(L"findpid pid=%lu", pid);
    if (!pid) {
        MessageBoxW(nullptr, L"Game belum jalan. Buka Delta Force dulu, lalu jalankan lagi.", L"DF-MENU", MB_ICONWARNING);
        return 2;
    }
    DWORD err = 0, openErr = 0, fbErr = 0;
    HANDLE hp = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                            PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    openErr = hp ? 0 : GetLastError();
    Logf(L"open ok=%d err=%lu", hp ? 1 : 0, openErr);
    bool ok = hp && ManualMap(hp, core, &err);
    Logf(L"manualmap ok=%d err=%lu step=%ld", ok ? 1 : 0, err, g_mapStep);
    bool fallback = false;
    if (!ok && hp) { ok = Inject(pid, core, &fbErr); fallback = ok; Logf(L"fallback ok=%d err=%lu", ok ? 1 : 0, fbErr); }
    if (hp) CloseHandle(hp);
    Logf(L"result ok=%d fallback=%d", ok ? 1 : 0, fallback ? 1 : 0);
    STARTUPINFOW si{ sizeof(si) }; PROCESS_INFORMATION pi{};
    if (ok) CreateProcessW(ovl, nullptr, nullptr, nullptr, FALSE, 0, nullptr, dir, &si, &pi);
    if (ok) MessageBoxW(nullptr, fallback ? L"Manual-map gagal, fallback LoadLibrary OK.\nOverlay jalan; INSERT menu."
                                          : L"Manual-map OK + overlay jalan.\nINSERT untuk menu.", L"DF-MENU", MB_ICONINFORMATION);
    else {
        HANDLE tok = nullptr; bool elevated = false;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
            TOKEN_ELEVATION te{}; DWORD sz = 0;
            if (GetTokenInformation(tok, TokenElevation, &te, sizeof(te), &sz)) elevated = te.TokenIsElevated != 0;
            CloseHandle(tok);
        }
        wchar_t b[384];
        wsprintfW(b, L"Inject gagal.\nelevated=%s\nopen=%lu manual=%lu fallback=%lu\n\nDetail: inject.log",
                  elevated ? L"yes" : L"no", openErr, err, fbErr);
        MessageBoxW(nullptr, b, L"DF-MENU", MB_ICONERROR);
    }
    return ok ? 0 : 3;
}
