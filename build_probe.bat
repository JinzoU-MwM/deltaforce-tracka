@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V15 /DPROBE_ONLY /D_USRDLL /D_WINDLL core.cpp /Fe:dfbodyprobe.dll /link /DLL user32.lib > probe.log 2>&1
echo PROBE=%errorlevel%
type probe.log
