@echo off
cd /d C:\Users\User\trackA
taskkill /IM dfoverlay10.exe /F >nul 2>&1
taskkill /IM dfoverlay11.exe /F >nul 2>&1
schtasks /create /tn dfa_c21 /tr "C:\Users\User\trackA\dfsecinject.exe 50552 C:\Users\User\trackA\dfcore11.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_c21
ping -n 5 127.0.0.1 >nul
schtasks /delete /tn dfa_c21 /f >nul
schtasks /create /tn dfa_o21 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay11.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o21
ping -n 5 127.0.0.1 >nul
schtasks /delete /tn dfa_o21 /f >nul
del shm_now.log 2>nul
schtasks /create /tn dfa_r21 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shmread11.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_r21
ping -n 6 127.0.0.1 >nul
schtasks /delete /tn dfa_r21 /f >nul
type shm_now.log 2>nul
