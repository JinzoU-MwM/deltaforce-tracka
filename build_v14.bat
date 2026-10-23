@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD /DSHM_V14 core.cpp /Fe:dfcore14.dll /link user32.lib > core14.log 2>&1
echo C14=%errorlevel%
type core14.log
taskkill /IM dfoverlay14.exe /F >nul 2>&1
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V14 overlay.cpp /Fe:dfoverlay14.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov14.log 2>&1
echo O14=%errorlevel%
type ov14.log
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V14 shmread.cpp /Fe:shmread14.exe > r14.log 2>&1
echo R14=%errorlevel%
