@echo off
cd /d C:\Users\User\trackA
taskkill /IM dfoverlay11.exe /F >nul 2>&1
taskkill /IM dfoverlay12.exe /F >nul 2>&1
ping -n 2 127.0.0.1 >nul
schtasks /create /tn dfa_o90 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay12.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o90
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_o90 /f >nul
del shm_now.log 2>nul
schtasks /create /tn dfa_r90 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shmread12.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_r90
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_r90 /f >nul
schtasks /create /tn dfa_sh90 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shot.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_sh90
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_sh90 /f >nul
type shm_now.log | findstr /C:"state=" /C:"cam mode"
