@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD /DSHM_V9 core.cpp /Fe:dfcore9.dll /link user32.lib > core9.log 2>&1
echo C9=%errorlevel%
type core9.log
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V9 overlay.cpp /Fe:dfoverlay9.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov9.log 2>&1
echo O9=%errorlevel%
