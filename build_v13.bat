@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD /DSHM_V13 core.cpp /Fe:dfcore13.dll /link user32.lib > core13.log 2>&1
echo C13=%errorlevel%
type core13.log
taskkill /IM dfoverlay13.exe /F >nul 2>&1
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V13 overlay.cpp /Fe:dfoverlay13.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov13.log 2>&1
echo O13=%errorlevel%
type ov13.log
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V13 shmread.cpp /Fe:shmread13.exe > r13.log 2>&1
echo R13=%errorlevel%
type r13.log
