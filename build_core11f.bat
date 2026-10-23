@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD /DSHM_V11 core.cpp /Fe:dfcore11.dll /link user32.lib > core11f.log 2>&1
echo C11F=%errorlevel%
type core11f.log
