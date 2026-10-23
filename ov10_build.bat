@echo off
taskkill /IM dfoverlay10.exe /F >nul 2>&1
taskkill /IM dfoverlay9.exe /F >nul 2>&1
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V10 overlay.cpp /Fe:dfoverlay10.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov10.log 2>&1
echo O10=%errorlevel%
type ov10.log
