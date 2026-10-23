@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfdump.cpp /Fe:dfdump.dll /link user32.lib > dump_build.log 2>&1
echo DUMP=%errorlevel%
type dump_build.log
