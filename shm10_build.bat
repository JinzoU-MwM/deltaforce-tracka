@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V10 shmread.cpp /Fe:shmread10.exe > r10.log 2>&1
echo R10=%errorlevel%
type r10.log
