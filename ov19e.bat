@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
taskkill /IM dfoverlay19.exe /F >nul 2>&1
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V19 overlay.cpp /Fe:dfoverlay19.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov19e.log 2>&1
echo OV19E=%errorlevel%
type ov19e.log
