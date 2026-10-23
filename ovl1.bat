@echo off
taskkill /IM dfoverlay10.exe /F >nul 2>&1
taskkill /IM dfoverlay9.exe /F >nul 2>&1
ping -n 2 127.0.0.1 >nul
schtasks /create /tn dfa_o14 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay10.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o14
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_o14 /f >nul
tasklist | findstr /I dfoverlay
