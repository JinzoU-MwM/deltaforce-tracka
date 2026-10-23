#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
int main() {
    HMODULE m = LoadLibraryA("dfcore.dll");
    printf("dll=%p\n", (void*)m);
    Sleep(2500);
    return 0;
}
