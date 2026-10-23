@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V17 /D_USRDLL /D_WINDLL core.cpp /Fe:dfcore17.dll /link /DLL user32.lib > c17.log 2>&1
echo C17=%errorlevel%
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V17 overlay.cpp /Fe:dfoverlay17.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov17.log 2>&1
echo OV17=%errorlevel%
type c17.log
type ov17.log
