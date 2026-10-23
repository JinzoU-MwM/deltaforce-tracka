@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
echo === core ===
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD core.cpp /Fe:dfcore.dll /link user32.lib > core.log 2>&1
echo CORE=%errorlevel%
type core.log
echo === overlay ===
cl /nologo /std:c++17 /O2 /EHsc overlay.cpp /Fe:dfoverlay.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov.log 2>&1
echo OVERLAY=%errorlevel%
type ov.log
echo === tools ===
cl /nologo /std:c++17 /O2 /EHsc shmread.cpp /Fe:shmread.exe > r.log 2>&1
echo READ=%errorlevel%
cl /nologo /std:c++17 /O2 /EHsc shmwrite.cpp /Fe:shmwrite.exe > w.log 2>&1
echo WRITE=%errorlevel%
cl /nologo /std:c++17 /O2 /EHsc host_core.cpp /Fe:host_core.exe > h.log 2>&1
echo HOST=%errorlevel%
