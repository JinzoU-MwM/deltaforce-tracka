#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <psapi.h>
#include <stdlib.h>
#include <wchar.h>
#pragma comment(lib,"psapi.lib")

typedef LONG NTSTATUS;
typedef struct _OBI { ULONG Attributes; ACCESS_MASK GrantedAccess; ULONG HandleCount, PointerCount;
    ULONG PagedPoolUsage, NonPagedPoolUsage; ULONG Reserved[3]; ULONG NameLen, TypeLen, SdLen; LARGE_INTEGER CreateTime; } OBI;
typedef struct _PBI { PVOID R1; PVOID PebBase; PVOID R2[2]; ULONG_PTR Pid; PVOID R3; } PBI;

static void Probe(DWORD pid, const wchar_t* tag) {
    FILE* lg = nullptr; wchar_t lp[MAX_PATH];
    wcscpy_s(lp, L"C:\\Users\\User\\trackA\\probe.log");
    _wfopen_s(&lg, lp, L"a");
#define LOGF(...) do { wprintf(__VA_ARGS__); if (lg) fwprintf(lg, __VA_ARGS__); } while(0)
    LOGF(L"[%s] pid=%lu\n", tag, (unsigned long)pid);

    auto NtQO = (NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG))GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryObject");
    auto NtQIP = (NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG))GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess");
    auto NtAVM = (NTSTATUS(NTAPI*)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG))GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtAllocateVirtualMemory");
    auto NtCTE = (NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, PVOID, HANDLE, PVOID, PVOID, ULONG, SIZE_T, SIZE_T, SIZE_T, PVOID))GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtCreateThreadEx");

    const ACCESS_MASK want = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ;
    HANDLE h = OpenProcess(want, FALSE, pid);
    LOGF(L"OpenProcess want=0x%lX h=%p err=%lu\n", (unsigned long)want, h, h ? 0 : GetLastError());
    if (!h) { LOGF(L"open failed err=%lu\n", GetLastError()); if (lg) fclose(lg); return; }
    OBI obi{}; ULONG len = 0;
    NTSTATUS st = NtQO(h, 0, &obi, sizeof(obi), &len);
    LOGF(L"NtQueryObject st=0x%08lX granted=0x%lX  (VM_OP=%d VM_WRITE=%d VM_READ=%d THREAD=%d QUERY=%d)\n",
        (unsigned long)st, (unsigned long)obi.GrantedAccess,
        (obi.GrantedAccess & PROCESS_VM_OPERATION) ? 1 : 0,
        (obi.GrantedAccess & PROCESS_VM_WRITE) ? 1 : 0,
        (obi.GrantedAccess & PROCESS_VM_READ) ? 1 : 0,
        (obi.GrantedAccess & PROCESS_CREATE_THREAD) ? 1 : 0,
        (obi.GrantedAccess & PROCESS_QUERY_INFORMATION) ? 1 : 0);
    PBI pbi{}; len = 0;
    st = NtQIP(h, 0, &pbi, sizeof(pbi), &len);
    LOGF(L"NtQueryInformationProcess st=0x%08lX peb=%p\n", (unsigned long)st, pbi.PebBase);
    void* base = nullptr; SIZE_T rd = 0;
    if (pbi.PebBase) {
        BYTE img[8]{}; SIZE_T got = 0;
        BOOL ok = ReadProcessMemory(h, (BYTE*)pbi.PebBase + 0x10, img, 8, &got);
        LOGF(L"RPM(PEB.ImageBase) ok=%d err=%lu got=%llu val=0x%llX\n", ok, ok ? 0 : GetLastError(), (unsigned long long)got, *(unsigned long long*)img);
        if (ok) {
            base = (void*)*(unsigned long long*)img;
            BYTE mz[2]{}; got = 0; ok = ReadProcessMemory(h, base, mz, 2, &got);
            LOGF(L"RPM(ImageBase) ok=%d err=%lu bytes=%02X %02X\n", ok, ok ? 0 : GetLastError(), mz[0], mz[1]);
        }
    }
    PVOID ap = nullptr; SIZE_T asz = 0x1000;
    st = NtAVM(h, &ap, 0, &asz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    LOGF(L"NtAllocateVirtualMemory st=0x%08lX addr=%p size=%llu\n", (unsigned long)st, ap, (unsigned long long)asz);
    if (ap) { SIZE_T fsz = 0; NtAVM(h, &ap, 0, &fsz, MEM_RELEASE, PAGE_NOACCESS); }
    HANDLE th = nullptr;
    st = NtCTE(&th, 0x1FFFFF, nullptr, h, nullptr, nullptr, 0, 0, 0, 0, nullptr);
    LOGF(L"NtCreateThreadEx st=0x%08lX thread=%p\n", (unsigned long)st, th);
    if (th) CloseHandle(th);
    HMODULE self[256]; DWORD need = 0;
    if (EnumProcessModules(GetCurrentProcess(), self, sizeof(self), &need)) {
        wchar_t nm[MAX_PATH]; bool ace = false;
        for (DWORD i = 0; i < need / sizeof(HMODULE); ++i) {
            if (GetModuleBaseNameW(GetCurrentProcess(), self[i], nm, MAX_PATH) && wcsstr(nm, L"ACE")) { LOGF(L"own module ACE: %s\n", nm); ace = true; }
        }
        if (!ace) LOGF(L"own modules: no ACE dll\n");
    }
    CloseHandle(h);
    if (lg) fclose(lg);
#undef OUT
}

extern "C" __declspec(dllexport) void CALLBACK Run(HWND, HINSTANCE, LPSTR cmd, int) {
    Probe((DWORD)atoi(cmd), L"rundll32");
}

