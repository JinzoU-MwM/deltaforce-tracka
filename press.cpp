#define WIN32_LEAN_AND_MEAN
#include <windows.h>
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){ keybd_event(VK_F8,0,0,0); Sleep(300); keybd_event(VK_F8,0,KEYEVENTF_KEYUP,0); return 0; }
