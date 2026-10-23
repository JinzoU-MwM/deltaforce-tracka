#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <psapi.h>
#include <stdlib.h>
#pragma comment(lib,"psapi.lib")

typedef LONG NTSTATUS;
typedef struct _OBI { ULONG Attributes; ACCESS_MASK GrantedAccess; ULONG HandleCount, PointerCount;
    ULONG PagedPoolUsage, NonPagedPoolUsage; ULONG Reserved[3]; ULONG NameLen, TypeLen, SdLen; LARGE_INTEGER CreateTime; } OBI;
typedef struct _PBI { PVOID R1; PVOID PebBase; PVOID R2[2]; ULONG_PTR Pid; PVOID R3; } PBI;

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) { wprintf(L"usage: dfacl <pid>\n"); return 1; }
    DWORD pid = (DWORD)_wtoi(argv[1]);
    auto NtQO = (NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG))GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryObject");
    auto NtQIP = (NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG))GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess");
    auto NtAVM = (NTSTATUS(NTAPI*)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG))GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtAllocateVirtualMemory");
    auto NtCTE = (NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, PVOID, HANDLE, PVOID, PVOID, ULONG, SIZE_T, SIZE_T, SIZE_T, PVOID))GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtCreateThreadEx");

    const ACCESS_MASK want = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ;
    HANDLE h = OpenProcess(want, FALSE, pid);
    wprintf(L"OpenProcess want=0x%lX h=%p err=%lu\n", (unsigned long)want, h, h ? 0 : GetLastError());
    if (!h) return 2;
    OBI obi{}; ULONG len = 0;
    NTSTATUS st = NtQO(h, 0, &obi, sizeof(obi), &len);
    wprintf(L"NtQueryObject st=0x%08lX granted=0x%lX  (VM_OP=%d VM_WRITE=%d VM_READ=%d THREAD=%d QUERY=%d)\n",
        (unsigned long)st, (unsigned long)obi.GrantedAccess,
        (obi.GrantedAccess & PROCESS_VM_OPERATION) ? 1 : 0,
        (obi.GrantedAccess & PROCESS_VM_WRITE) ? 1 : 0,
        (obi.GrantedAccess & PROCESS_VM_READ) ? 1 : 0,
        (obi.GrantedAccess & PROCESS_CREATE_THREAD) ? 1 : 0,
        (obi.GrantedAccess & PROCESS_QUERY_INFORMATION) ? 1 : 0);
    PBI pbi{}; len = 0;
    st = NtQIP(h, 0, &pbi, sizeof(pbi), &len);
    wprintf(L"NtQueryInformationProcess st=0x%08lX peb=%p\n", (unsigned long)st, pbi.PebBase);
    void* base = nullptr; SIZE_T rd = 0;
    if (pbi.PebBase) {
        BYTE img[8]{}; SIZE_T got = 0;
        BOOL ok = ReadProcessMemory(h, (BYTE*)pbi.PebBase + 0x10, img, 8, &got);
        wprintf(L"RPM(PEB.ImageBase) ok=%d err=%lu got=%llu val=0x%llX\n", ok, ok ? 0 : GetLastError(), (unsigned long long)got, *(unsigned long long*)img);
        if (ok) {
            base = (void*)*(unsigned long long*)img;
            BYTE mz[2]{}; got = 0; ok = ReadProcessMemory(h, base, mz, 2, &got);
            wprintf(L"RPM(ImageBase) ok=%d err=%lu bytes=%02X %02X\n", ok, ok ? 0 : GetLastError(), mz[0], mz[1]);
        }
    }
    PVOID ap = nullptr; SIZE_T asz = 0x1000;
    st = NtAVM(h, &ap, 0, &asz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    wprintf(L"NtAllocateVirtualMemory st=0x%08lX addr=%p size=%llu\n", (unsigned long)st, ap, (unsigned long long)asz);
    if (ap) { SIZE_T fsz = 0; NtAVM(h, &ap, 0, &fsz, MEM_RELEASE, PAGE_NOACCESS); }
    HANDLE th = nullptr;
    st = NtCTE(&th, 0x1FFFFF, nullptr, h, nullptr, nullptr, 0, 0, 0, 0, nullptr);
    wprintf(L"NtCreateThreadEx st=0x%08lX thread=%p\n", (unsigned long)st, th);
    if (th) CloseHandle(th);
    HMODULE self[256]; DWORD need = 0;
    if (EnumProcessModules(GetCurrentProcess(), self, sizeof(self), &need)) {
        wchar_t nm[MAX_PATH]; bool ace = false;
        for (DWORD i = 0; i < need / sizeof(HMODULE); ++i) {
            if (GetModuleBaseNameW(GetCurrentProcess(), self[i], nm, MAX_PATH) && wcsstr(nm, L"ACE")) { wprintf(L"own module ACE: %s\n", nm); ace = true; }
        }
        if (!ace) wprintf(L"own modules: no ACE dll\n");
    }
    CloseHandle(h);
    return 0;
}
