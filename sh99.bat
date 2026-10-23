@echo off
cd /d C:\Users\User\trackA
echo -40 0 0 > camcfg.txt
ping -n 5 127.0.0.1 >nul
schtasks /create /tn dfa_sh99 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shot.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_sh99
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_sh99 /f >nul
type camcfg.txt
