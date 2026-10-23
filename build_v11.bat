@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD /DSHM_V11 core.cpp /Fe:dfcore11.dll /link user32.lib > core11.log 2>&1
echo C11=%errorlevel%
type core11.log
taskkill /IM dfoverlay11.exe /F >nul 2>&1
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V11 overlay.cpp /Fe:dfoverlay11.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov11.log 2>&1
echo O11=%errorlevel%
type ov11.log
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V11 shmread.cpp /Fe:shmread11.exe > r11.log 2>&1
echo R11=%errorlevel%
type r11.log
