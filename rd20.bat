@echo off
cd /d C:\Users\User\trackA
del shm_now.log 2>nul
schtasks /create /tn dfa_r20 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shmread10.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_r20
ping -n 6 127.0.0.1 >nul
schtasks /delete /tn dfa_r20 /f >nul
type shm_now.log
