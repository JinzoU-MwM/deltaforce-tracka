@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc dfsecinject.cpp /Fe:dfsecinject.exe /link user32.lib > sec_build.log 2>&1
echo SEC=%errorlevel%
type sec_build.log
