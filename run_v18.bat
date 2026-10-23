@echo off
cd /d C:\Users\User\trackA
taskkill /IM dfoverlay18.exe /F >nul 2>&1
schtasks /create /tn dfa_c18 /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfcore18.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_c18
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_c18 /f >nul
schtasks /create /tn dfa_o18 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay18.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o18
ping -n 3 127.0.0.1 >nul
schtasks /delete /tn dfa_o18 /f >nul
tasklist | findstr /I dfoverlay
