@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfdumpfunc.cpp /Fe:dfdumpfunc.dll /link user32.lib > dfunc_build.log 2>&1
echo DFB=%errorlevel%
schtasks /create /tn dfa_df /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfdumpfunc.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_df
ping -n 8 127.0.0.1 >nul
schtasks /delete /tn dfa_df /f >nul
type funcb.txt
