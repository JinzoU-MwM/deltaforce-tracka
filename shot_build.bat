@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc shot.cpp /Fe:shot.exe /link gdiplus.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > shot.log 2>&1
echo SHOT=%errorlevel%
type shot.log
