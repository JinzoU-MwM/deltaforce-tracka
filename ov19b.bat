@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
taskkill /IM dfoverlay19.exe /F >nul 2>&1
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V19 overlay.cpp /Fe:dfoverlay19.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov19b.log 2>&1
echo OV19B=%errorlevel%
type ov19b.log
schtasks /create /tn dfa_o19b /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay19.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o19b
ping -n 3 127.0.0.1 >nul
schtasks /delete /tn dfa_o19b /f >nul
tasklist | findstr /I dfoverlay
