@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD /DSHM_V15 core.cpp /Fe:dfcore15.dll /link user32.lib > core15.log 2>&1
echo C15=%errorlevel%
type core15.log
taskkill /IM dfoverlay15.exe /F >nul 2>&1
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V15 overlay.cpp /Fe:dfoverlay15.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov15.log 2>&1
echo O15=%errorlevel%
type ov15.log
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V15 shmread.cpp /Fe:shmread15.exe > r15.log 2>&1
echo R15=%errorlevel%
