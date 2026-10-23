@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V18 /D_USRDLL /D_WINDLL core.cpp /Fe:dfcore18.dll /link /DLL user32.lib > c18.log 2>&1
echo C18=%errorlevel%
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V18 overlay.cpp /Fe:dfoverlay18.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov18.log 2>&1
echo O18=%errorlevel%
type c18.log
type ov18.log
