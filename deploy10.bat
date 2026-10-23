@echo off
cd /d C:\Users\User\trackA
taskkill /IM dfoverlay9.exe /F >nul 2>&1
taskkill /IM dfoverlay10.exe /F >nul 2>&1
del shm_now.log 2>nul
copy /y dfcore10.dll dfcore.dll >nul
dfrun.exe --test --pid 50552 > coremap10.log 2>&1
type coremap10.log
schtasks /create /tn dfa_o10 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay10.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o10
ping -n 4 127.0.0.1 >nul
schtasks /create /tn dfa_r10 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shmread10.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_r10
ping -n 5 127.0.0.1 >nul
schtasks /delete /tn dfa_o10 /f >nul
schtasks /delete /tn dfa_r10 /f >nul
type shm_now.log 2>nul
