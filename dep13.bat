@echo off
cd /d C:\Users\User\trackA
schtasks /create /tn dfa_c13 /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfcore13.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_c13
ping -n 5 127.0.0.1 >nul
schtasks /delete /tn dfa_c13 /f >nul
taskkill /IM dfoverlay11.exe /F >nul 2>&1
taskkill /IM dfoverlay13.exe /F >nul 2>&1
schtasks /create /tn dfa_o13 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay13.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o13
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_o13 /f >nul
del shm_now.log 2>nul
schtasks /create /tn dfa_r13 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shmread13.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_r13
ping -n 3 127.0.0.1 >nul
schtasks /delete /tn dfa_r13 /f >nul
schtasks /create /tn dfa_s13 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shot.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_s13
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_s13 /f >nul
type shm_now.log | findstr /C:"state=" /C:"cam mode" /C:"p0 " /C:"p1 " /C:"p2 "
