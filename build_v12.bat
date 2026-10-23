@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD /DSHM_V12 core.cpp /Fe:dfcore12.dll /link user32.lib > core12.log 2>&1
echo C12=%errorlevel%
type core12.log
taskkill /IM dfoverlay12.exe /F >nul 2>&1
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V12 overlay.cpp /Fe:dfoverlay12.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov12.log 2>&1
echo O12=%errorlevel%
type ov12.log
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V12 shmread.cpp /Fe:shmread12.exe > r12.log 2>&1
echo R12=%errorlevel%
type r12.log
