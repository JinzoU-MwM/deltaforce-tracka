@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfdump2.cpp /Fe:dfdump2.dll /link user32.lib > dump2_build.log 2>&1
echo D2=%errorlevel%
type dump2_build.log
