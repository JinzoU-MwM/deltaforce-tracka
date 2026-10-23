@echo off
cd /d C:\Users\User\trackA
schtasks /create /tn dfa_oG /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay11.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_oG
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_oG /f >nul
type camcfg.txt
