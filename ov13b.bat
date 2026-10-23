@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
taskkill /IM dfoverlay13.exe /F >nul 2>&1
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V13 overlay.cpp /Fe:dfoverlay13.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov13b.log 2>&1
echo O13B=%errorlevel%
echo 0 0 0 90 > camcfg.txt
schtasks /create /tn dfa_o13b /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay13.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o13b
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_o13b /f >nul
type camcfg.txt
