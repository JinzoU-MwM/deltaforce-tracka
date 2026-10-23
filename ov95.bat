@echo off
cd /d C:\Users\User\trackA
taskkill /IM dfoverlay12.exe /F >nul 2>&1
taskkill /IM dfoverlay11.exe /F >nul 2>&1
ping -n 2 127.0.0.1 >nul
schtasks /create /tn dfa_o95 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay11.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o95
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_o95 /f >nul
tasklist | findstr /I dfoverlay
