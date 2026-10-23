@echo off
cd /d C:\Users\User\trackA
echo 0 0 0 28 > camcfg.txt
ping -n 2 127.0.0.1 >nul
del shm_now.log 2>nul
schtasks /create /tn dfa_rA /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shmread13.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_rA
ping -n 2 127.0.0.1 >nul
schtasks /delete /tn dfa_rA /f >nul
schtasks /create /tn dfa_sB /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shot.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_sB
ping -n 3 127.0.0.1 >nul
schtasks /delete /tn dfa_sB /f >nul
type shm_now.log | findstr /C:"cam mode" /C:"p0 " /C:"p1 " /C:"p2 " /C:"p3 "
