@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
taskkill /IM dfoverlay13.exe /F >nul 2>&1
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V13 overlay.cpp /Fe:dfoverlay13.exe /link d2d1.lib dwrite.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov13c.log 2>&1
echo O13C=%errorlevel%
schtasks /create /tn dfa_o13c /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay13.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o13c
ping -n 3 127.0.0.1 >nul
schtasks /delete /tn dfa_o13c /f >nul
del shm_now.log 2>nul
schtasks /create /tn dfa_rC /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shmread13.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /create /tn dfa_sC /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\shot.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_rC
schtasks /run /tn dfa_sC
ping -n 4 127.0.0.1 >nul
schtasks /delete /tn dfa_rC /f >nul
schtasks /delete /tn dfa_sC /f >nul
type shm_now.log | findstr /C:"cam mode" /C:"p0 " /C:"p1 " /C:"p2 " /C:"p3 " /C:"p4 " /C:"p5 "
