@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc dfthread2.cpp /Fe:dfthread2.exe > t2.log 2>&1
echo T2=%errorlevel%
type t2.log
