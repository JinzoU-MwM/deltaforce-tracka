@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V11 shmread.cpp /Fe:shmread11.exe > r11.log 2>&1
echo R11=%errorlevel%
del shm_now.log 2>nul
schtasks /create /tn dfa_r30 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shmread11.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_r30
ping -n 6 127.0.0.1 >nul
schtasks /delete /tn dfa_r30 /f >nul
type shm_now.log
