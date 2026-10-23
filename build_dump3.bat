@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfdump3.cpp /Fe:dfdump3.dll /link user32.lib > dump3_build.log 2>&1
echo D3=%errorlevel%
type dump3_build.log
