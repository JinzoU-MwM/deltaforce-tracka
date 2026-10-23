#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
static DWORD WINAPI Th(LPVOID){
    wchar_t exe[MAX_PATH]{}; GetModuleFileNameW((HMODULE)GetModuleHandleW(nullptr), exe, MAX_PATH);
    FILE* f=nullptr; _wfopen_s(&f,L"C:\\Users\\User\\trackA\\path.txt",L"w"); if(!f) return 1;
    wchar_t dir[MAX_PATH]{}; wcscpy_s(dir, exe); wchar_t* s=wcsrchr(dir,L'\\'); if(s) *s=0;
    wchar_t pdb[MAX_PATH]{}; wcscpy_s(pdb, exe); wchar_t* d=wcsrchr(pdb,L'.'); if(d) wcscpy_s(d, MAX_PATH-(d-pdb), L".pdb");
    fwprintf(f,L"EXE=%ls\nPDB=%ls exists=%d\n",exe,pdb,GetFileAttributesW(pdb)!=INVALID_FILE_ATTRIBUTES);
    WIN32_FIND_DATAW fd{}; wchar_t pat[MAX_PATH]{}; swprintf_s(pat,L"%ls\\*.pdb",dir);
    HANDLE h=FindFirstFileW(pat,&fd);
    if(h!=INVALID_HANDLE_VALUE){ do{ fwprintf(f,L"FOUND %ls %llu\n",fd.cFileName,((unsigned long long)fd.nFileSizeHigh<<32)|fd.nFileSizeLow);}while(FindNextFileW(h,&fd)); FindClose(h);} else fwprintf(f,L"NO_PDB_FILES\n");
    fclose(f); return 0;
}
BOOL APIENTRY DllMain(HMODULE,DWORD r,LPVOID){ if(r==DLL_PROCESS_ATTACH) CreateThread(nullptr,0,Th,nullptr,0,nullptr); return TRUE; }
