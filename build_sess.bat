@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc sessrun.cpp /Fe:sessrun.exe /link wtsapi32.lib advapi32.lib > sess_build.log 2>&1
echo SESS=%errorlevel%
type sess_build.log
