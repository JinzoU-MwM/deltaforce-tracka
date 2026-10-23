@echo off
cd /d C:\Users\User\trackA
schtasks /create /tn dfa_shot2 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shot.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_shot2
ping -n 5 127.0.0.1 >nul
schtasks /delete /tn dfa_shot2 /f >nul
type shot_ok.txt
