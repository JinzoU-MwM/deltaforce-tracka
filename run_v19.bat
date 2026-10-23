@echo off
cd /d C:\Users\User\trackA
taskkill /IM dfoverlay19.exe /F >nul 2>&1
schtasks /create /tn dfa_c19 /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfcore19.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_c19
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_c19 /f >nul
schtasks /create /tn dfa_o19 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay19.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o19
ping -n 3 127.0.0.1 >nul
schtasks /delete /tn dfa_o19 /f >nul
tasklist | findstr /I dfoverlay
