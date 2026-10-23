@echo off
cd /d C:\Users\User\trackA
schtasks /create /tn dfa_c14 /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfcore14.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_c14
ping -n 5 127.0.0.1 >nul
schtasks /delete /tn dfa_c14 /f >nul
taskkill /IM dfoverlay13.exe /F >nul 2>&1
taskkill /IM dfoverlay14.exe /F >nul 2>&1
schtasks /create /tn dfa_o14 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay14.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o14
ping -n 3 127.0.0.1 >nul
schtasks /delete /tn dfa_o14 /f >nul
del shm_now.log 2>nul
schtasks /create /tn dfa_rD /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shmread14.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /create /tn dfa_sD /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shot.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_rD
schtasks /run /tn dfa_sD
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_rD /f >nul
schtasks /delete /tn dfa_sD /f >nul
type shm_now.log | findstr /C:"cam mode" /C:"p0 " /C:"p1 " /C:"p2 " /C:"p3 " /C:"p4 " /C:"p5 "
