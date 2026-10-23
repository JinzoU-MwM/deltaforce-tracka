@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /MT /GS- /LD dfstr.cpp /Fe:dfstr.dll /link user32.lib > str_build.log 2>&1
echo STRB2=%errorlevel%
schtasks /create /tn dfa_str2 /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfstr.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_str2
ping -n 12 127.0.0.1 >nul
schtasks /delete /tn dfa_str2 /f >nul
type str.txt | findstr /C:"XREF" /C:"STRN"
