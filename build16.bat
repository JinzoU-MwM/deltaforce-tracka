@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V16 /D_USRDLL /D_WINDLL core.cpp /Fe:dfcore16.dll /link /DLL user32.lib > c16.log 2>&1
echo C16=%errorlevel%
type c16.log
