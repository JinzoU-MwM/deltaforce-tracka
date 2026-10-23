@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfpath.cpp /Fe:dfpath.dll /link user32.lib > path_build.log 2>&1
echo PB=%errorlevel%
type path_build.log
schtasks /create /tn dfa_path /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfpath.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_path
ping -n 6 127.0.0.1 >nul
schtasks /delete /tn dfa_path /f >nul
type path.txt
