@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfdiag.cpp /Fe:dfdiag.dll /link user32.lib > diag.log 2>&1
echo DIAG=%errorlevel%
type diag.log
