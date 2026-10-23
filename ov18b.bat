@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
taskkill /IM dfoverlay18.exe /F >nul 2>&1
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V18 overlay.cpp /Fe:dfoverlay18.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov18b.log 2>&1
echo OV18B=%errorlevel%
type ov18b.log
schtasks /create /tn dfa_o18b /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay18.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o18b
ping -n 3 127.0.0.1 >nul
schtasks /delete /tn dfa_o18b /f >nul
tasklist | findstr /I dfoverlay
