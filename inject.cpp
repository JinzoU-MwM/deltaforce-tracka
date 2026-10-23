#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <wchar.h>

static DWORD FindPid(const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{ sizeof(pe) };
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

int wmain(int argc, wchar_t** argv) {
    if (argc < 3) {
        wprintf(L"usage: dfinject.exe <pid|exe-name> <dll-path>\n");
        return 1;
    }
    DWORD pid = 0;
    if (iswdigit(argv[1][0])) pid = (DWORD)_wtoi(argv[1]);
    if (!pid) pid = FindPid(argv[1]);
    if (!pid) { wprintf(L"process not found\n"); return 2; }

    HANDLE h = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                           PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                           FALSE, pid);
    if (!h) { wprintf(L"OpenProcess failed %lu\n", GetLastError()); return 3; }

    SIZE_T n = (wcslen(argv[2]) + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(h, nullptr, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote) { wprintf(L"VirtualAllocEx failed %lu\n", GetLastError()); return 4; }
    if (!WriteProcessMemory(h, remote, argv[2], n, nullptr)) {
        wprintf(L"WriteProcessMemory failed %lu\n", GetLastError()); return 5;
    }
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    void* load = (void*)GetProcAddress(k32, "LoadLibraryW");
    HANDLE t = CreateRemoteThread(h, nullptr, 0, (LPTHREAD_START_ROUTINE)load, remote, 0, nullptr);
    if (!t) { wprintf(L"CreateRemoteThread failed %lu\n", GetLastError()); return 6; }
    WaitForSingleObject(t, 10000);
    DWORD ec = 0;
    GetExitCodeThread(t, &ec);
    wprintf(L"injected pid=%lu thread_exit=%lu\n", pid, ec);
    return ec ? 0 : 7;
}
