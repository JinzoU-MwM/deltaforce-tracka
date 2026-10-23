@echo off
cd /d C:\Users\User\trackA
schtasks /create /tn dfa_c15 /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfcore15.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_c15
ping -n 5 127.0.0.1 >nul
schtasks /delete /tn dfa_c15 /f >nul
taskkill /IM dfoverlay14.exe /F >nul 2>&1
taskkill /IM dfoverlay15.exe /F >nul 2>&1
schtasks /create /tn dfa_o15 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay15.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o15
ping -n 3 127.0.0.1 >nul
schtasks /delete /tn dfa_o15 /f >nul
del snap.txt 2>nul
schtasks /create /tn dfa_p15 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\press.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_p15
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_p15 /f >nul
type snap.txt
