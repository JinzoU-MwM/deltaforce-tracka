@echo off
cd /d C:\Users\User\trackA
del shm_now.log 2>nul
schtasks /create /tn dfa_rG /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shmread11.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_rG
ping -n 3 127.0.0.1 >nul
schtasks /delete /tn dfa_rG /f >nul
schtasks /create /tn dfa_sG /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shot.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_sG
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_sG /f >nul
type shm_now.log | findstr /C:"state=" /C:"cam mode" /C:"p0 " /C:"p1 " /C:"p2 "
