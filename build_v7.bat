@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD /DSHM_V7 core.cpp /Fe:dfcore7.dll /link user32.lib > core7.log 2>&1
echo C7=%errorlevel%
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V7 overlay.cpp /Fe:dfoverlay7.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov7.log 2>&1
echo O7=%errorlevel%
