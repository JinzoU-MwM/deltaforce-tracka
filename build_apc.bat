@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc dfapc.cpp /Fe:dfapc.exe > apc_build.log 2>&1
echo APC=%errorlevel%
type apc_build.log
