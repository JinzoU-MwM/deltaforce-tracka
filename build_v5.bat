@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD /DSHM_V5 core.cpp /Fe:dfcore5.dll /link user32.lib > core5.log 2>&1
echo C5=%errorlevel%
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V5 overlay.cpp /Fe:dfoverlay5.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov5.log 2>&1
echo O5=%errorlevel%
