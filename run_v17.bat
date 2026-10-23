@echo off
cd /d C:\Users\User\trackA
taskkill /IM dfoverlay17.exe /F >nul 2>&1
schtasks /create /tn dfa_c17 /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfcore17.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_c17
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_c17 /f >nul
schtasks /create /tn dfa_o17 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay17.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o17
ping -n 3 127.0.0.1 >nul
schtasks /delete /tn dfa_o17 /f >nul
tasklist | findstr /I dfoverlay
