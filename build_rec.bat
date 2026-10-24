@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" > nul 2>&1
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /D_USRDLL /D_WINDLL /I . dfrec.cpp /Fe:dfrec.dll /link /DLL user32.lib > rec_build.log 2>&1
echo REC=%errorlevel% >> rec_build.log
