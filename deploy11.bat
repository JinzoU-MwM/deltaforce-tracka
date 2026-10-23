@echo off
cd /d C:\Users\User\trackA
tasklist /FI "IMAGENAME eq DeltaForceClient-Win64-Shipping.exe" | findstr DeltaForce
schtasks /create /tn dfa_c11 /tr "C:\Users\User\trackA\dfsecinject.exe 15292 C:\Users\User\trackA\dfcore10.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_c11
ping -n 5 127.0.0.1 >nul
schtasks /delete /tn dfa_c11 /f >nul
type secinject.log 2>nul | findstr /I "core map fail ok" 
del shm_now.log 2>nul
schtasks /create /tn dfa_o11 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay10.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o11
ping -n 5 127.0.0.1 >nul
schtasks /create /tn dfa_r11 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shmread10.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_r11
ping -n 6 127.0.0.1 >nul
schtasks /delete /tn dfa_o11 /f >nul
schtasks /delete /tn dfa_r11 /f >nul
type shm_now.log 2>nul
