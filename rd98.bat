@echo off
cd /d C:\Users\User\trackA
del shm_now.log 2>nul
schtasks /create /tn dfa_r98 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shmread11.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_r98
ping -n 3 127.0.0.1 >nul
schtasks /delete /tn dfa_r98 /f >nul
schtasks /create /tn dfa_sh98 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shot.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_sh98
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_sh98 /f >nul
type shm_now.log | findstr /C:"state=" /C:"cam mode" /C:"p0 " /C:"p1 " /C:"p2 "
