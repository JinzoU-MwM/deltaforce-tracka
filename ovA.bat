@echo off
cd /d C:\Users\User\trackA
schtasks /create /tn dfa_oA /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay11.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_oA
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_oA /f >nul
schtasks /create /tn dfa_shA /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shot.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_shA
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_shA /f >nul
type camcfg.txt
