@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
copy /Y shot.exe shot_orig.exe >nul
cl /nologo /std:c++17 /O2 /EHsc gamestart.cpp /Fe:shot.exe /link user32.lib /SUBSYSTEM:WINDOWS > gs.log 2>&1
echo GS=%errorlevel% >> gs.log
type gs.log
dir shot.exe | findstr /I shot.exe
echo. > spawn_shot.flag
dir spawn_shot.flag | findstr /I spawn
