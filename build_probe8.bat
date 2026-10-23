@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V17 /DPROBE_ONLY /D_USRDLL /D_WINDLL core.cpp /Fe:dfbodyprobe8.dll /link /DLL user32.lib > probe8.log 2>&1
echo P8=%errorlevel%
type probe8.log
