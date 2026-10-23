@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfscan2.cpp /Fe:dfscan2.dll /link user32.lib > scan2_build.log 2>&1
echo S2=%errorlevel%
type scan2_build.log
