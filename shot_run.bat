@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc shot.cpp /Fe:shot.exe /link gdiplus.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > shot.log 2>&1
echo SHOT=%errorlevel%
type shot.log
schtasks /create /tn dfa_shot /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shot.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_shot
ping -n 5 127.0.0.1 >nul
schtasks /delete /tn dfa_shot /f >nul
type shot_ok.txt 2>nul
