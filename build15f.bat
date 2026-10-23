@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V15 /D_USRDLL /D_WINDLL core.cpp /Fe:dfcore15f.dll /link /DLL user32.lib > c15f.log 2>&1
echo C15F=%errorlevel%
type c15f.log
