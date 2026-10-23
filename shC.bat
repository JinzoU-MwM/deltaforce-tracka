@echo off
cd /d C:\Users\User\trackA
echo 0 -300 0 67 > camcfg.txt
ping -n 4 127.0.0.1 >nul
schtasks /create /tn dfa_shC /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shot.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_shC
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_shC /f >nul
type camcfg.txt
