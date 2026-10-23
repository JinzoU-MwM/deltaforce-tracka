#include <windows.h>
int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    char cmd[] = "\"D:\\Delta Force\\Delta Force\\DeltaForceClient.exe\"";
    char cwd[] = "D:\\Delta Force\\Delta Force";
    if (CreateProcessA(nullptr, cmd, nullptr, nullptr, FALSE, 0, nullptr, cwd, &si, &pi)) {
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
    return 0;
}
