@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD /DSHM_V10 core.cpp /Fe:dfcore10.dll /link user32.lib > core10.log 2>&1
echo C10=%errorlevel%
type core10.log
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V10 overlay.cpp /Fe:dfoverlay10.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov10.log 2>&1
echo O10=%errorlevel%
