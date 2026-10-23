@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
taskkill /IM dfoverlay14.exe /F >nul 2>&1
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V14 overlay.cpp /Fe:dfoverlay14.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov14b.log 2>&1
echo O14B=%errorlevel%
type ov14b.log
cl /nologo /std:c++17 /O2 /EHsc press.cpp /Fe:press.exe /link user32.lib /SUBSYSTEM:WINDOWS > press.log 2>&1
echo PB=%errorlevel%
type press.log
schtasks /create /tn dfa_o14b /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay14.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o14b
ping -n 3 127.0.0.1 >nul
schtasks /delete /tn dfa_o14b /f >nul
schtasks /create /tn dfa_press /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\press.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_press
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_press /f >nul
type snap.txt
