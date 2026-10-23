@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
del body.txt 2>nul
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V15 /D_USRDLL /D_WINDLL core.cpp /Fe:dfcore15b.dll /link /DLL user32.lib > c15b.log 2>&1
echo C15B=%errorlevel%
type c15b.log
