@echo off
cd /d C:\Users\User\trackA
schtasks /create /tn dfa_sm3 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shot.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_sm3
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_sm3 /f >nul
