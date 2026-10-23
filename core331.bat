@echo off
cd /d C:\Users\User\trackA
schtasks /create /tn dfa_c331 /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfcore11.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_c331
ping -n 5 127.0.0.1 >nul
schtasks /delete /tn dfa_c331 /f >nul
taskkill /IM dfoverlay11.exe /F >nul 2>&1
schtasks /create /tn dfa_o331 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay11.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o331
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_o331 /f >nul
del shm_now.log 2>nul
schtasks /create /tn dfa_r331 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shmread11.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_r331
ping -n 5 127.0.0.1 >nul
schtasks /delete /tn dfa_r331 /f >nul
type shm_now.log
