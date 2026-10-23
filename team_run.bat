@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfteam.cpp /Fe:dfteam.dll /link user32.lib > team_build.log 2>&1
echo TB=%errorlevel%
type team_build.log
schtasks /create /tn dfa_team /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfteam.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_team
ping -n 8 127.0.0.1 >nul
schtasks /delete /tn dfa_team /f >nul
type team.txt
