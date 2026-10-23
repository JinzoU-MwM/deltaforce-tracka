@echo off
cd /d C:\Users\User\trackA
schtasks /create /tn dfa_ch2 /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfchars.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_ch2
ping -n 8 127.0.0.1 >nul
schtasks /delete /tn dfa_ch2 /f >nul
type chars.txt
