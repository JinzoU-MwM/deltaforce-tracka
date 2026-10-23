#include <windows.h>
int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    char exe[] = "C:\\Windows\\System32\\cmd.exe";
    char cmd[] = "/c C:\\Users\\User\\trackA\\StartOverlay.bat";
    char cwd[] = "C:\\Users\\User\\trackA";
    if (CreateProcessA(exe, cmd, nullptr, nullptr, FALSE, 0, nullptr, cwd, &si, &pi)) {
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
    return 0;
}
