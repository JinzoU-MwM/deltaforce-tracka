@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V19 /D_USRDLL /D_WINDLL core.cpp /Fe:dfcore19.dll /link /DLL user32.lib > c19.log 2>&1
echo C19=%errorlevel%
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V19 overlay.cpp /Fe:dfoverlay19.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov19.log 2>&1
echo O19=%errorlevel%
type c19.log
type ov19.log
