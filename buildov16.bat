@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V16 overlay.cpp /Fe:dfoverlay16.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov16.log 2>&1
echo OV16=%errorlevel%
type ov16.log
