@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfcall.cpp /Fe:dfcall.dll /link user32.lib > call_build.log 2>&1
echo CALLB=%errorlevel%
schtasks /create /tn dfa_call /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfcall.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_call
ping -n 8 127.0.0.1 >nul
schtasks /delete /tn dfa_call /f >nul
type calls.txt
