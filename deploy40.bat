@echo off
cd /d C:\Users\User\trackA
taskkill /IM dfoverlay11.exe /F >nul 2>&1
taskkill /IM dfoverlay12.exe /F >nul 2>&1
schtasks /create /tn dfa_c40 /tr "C:\Users\User\trackA\dfsecinject.exe 50552 C:\Users\User\trackA\dfcore12.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_c40
ping -n 5 127.0.0.1 >nul
schtasks /delete /tn dfa_c40 /f >nul
schtasks /create /tn dfa_o40 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay12.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o40
ping -n 5 127.0.0.1 >nul
schtasks /delete /tn dfa_o40 /f >nul
del shm_now.log 2>nul
schtasks /create /tn dfa_r40 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shmread12.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_r40
ping -n 5 127.0.0.1 >nul
schtasks /delete /tn dfa_r40 /f >nul
schtasks /create /tn dfa_sh40 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shot.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_sh40
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_sh40 /f >nul
type shm_now.log
