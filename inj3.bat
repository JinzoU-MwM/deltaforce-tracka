@echo off
cd /d C:\Users\User\trackA
copy /y dfoff2.dll dfoff3.dll >nul
tasklist /FI "IMAGENAME eq DeltaForceClient-Win64-Shipping.exe" | findstr DeltaForce
schtasks /create /tn dfa_off3 /tr "C:\Users\User\trackA\dfsecinject.exe 50552 C:\Users\User\trackA\dfoff3.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f
schtasks /run /tn dfa_off3
ping -n 5 127.0.0.1 >nul
schtasks /delete /tn dfa_off3 /f
type off2.txt
