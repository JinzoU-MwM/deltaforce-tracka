@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfcam2.cpp /Fe:dfcam2.dll /link user32.lib > cam2_build.log 2>&1
echo CAM2=%errorlevel%
type cam2_build.log
schtasks /create /tn dfa_cam2 /tr "C:\Users\User\trackA\dfsecinject.exe 50552 C:\Users\User\trackA\dfcam2.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_cam2
ping -n 6 127.0.0.1 >nul
schtasks /delete /tn dfa_cam2 /f >nul
type cam2.txt
