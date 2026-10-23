@echo off
cd /d C:\Users\User\trackA
schtasks /create /tn dfa_c12 /tr "C:\Users\User\trackA\dfsecinject.exe 50552 C:\Users\User\trackA\dfcore10.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_c12
ping -n 5 127.0.0.1 >nul
schtasks /delete /tn dfa_c12 /f >nul
del shm_now.log 2>nul
schtasks /create /tn dfa_o12 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay10.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o12
ping -n 5 127.0.0.1 >nul
schtasks /create /tn dfa_r12 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shmread10.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_r12
ping -n 6 127.0.0.1 >nul
schtasks /delete /tn dfa_o12 /f >nul
schtasks /delete /tn dfa_r12 /f >nul
type shm_now.log 2>nul
