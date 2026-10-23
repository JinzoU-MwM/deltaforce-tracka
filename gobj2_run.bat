@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfgobj2.cpp /Fe:dfgobj2.dll /link user32.lib > gobj2_build.log 2>&1
echo GB2=%errorlevel%
type gobj2_build.log
schtasks /create /tn dfa_g2 /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfgobj2.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_g2
ping -n 10 127.0.0.1 >nul
schtasks /delete /tn dfa_g2 /f >nul
type gobj2.txt
