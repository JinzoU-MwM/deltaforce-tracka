@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc inject.cpp /Fe:dfinject.exe /link user32.lib > inj.log 2>&1
echo INJECT_BUILD=%errorlevel%
type inj.log
