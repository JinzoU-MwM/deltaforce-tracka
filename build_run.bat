@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc dfrun.cpp /Fe:DF-MENU.exe /link user32.lib advapi32.lib /SUBSYSTEM:WINDOWS > run.log 2>&1
echo RUN_BUILD=%errorlevel%
type run.log
cl /nologo /std:c++17 /O2 /EHsc dfmap.cpp /Fe:dfmap.exe > map.log 2>&1
mt.exe -nologo -manifest dfrun.manifest "-outputresource:DF-MENU.exe;#1" > mt.log 2>&1
echo MT=%errorlevel%
type mt.log
