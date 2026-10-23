@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d C:\Users\User\trackA
cl /nologo /std:c++17 /O2 /EHsc /DSHM_V30 /D_USRDLL /D_WINDLL core.cpp /Fe:dfcore30.dll /link /DLL user32.lib > c30.log 2>&1
echo C30=%errorlevel%
type c30.log
cl /nologo /std:c++17 /O2 /EHsc /I imgui /I imgui\backends overlay_imgui.cpp imgui\imgui.cpp imgui\imgui_draw.cpp imgui\imgui_tables.cpp imgui\imgui_widgets.cpp imgui\backends\imgui_impl_win32.cpp imgui\backends\imgui_impl_dx11.cpp /Fe:dfimgui_overlay_multi.exe /link d3d11.lib dxgi.lib d3dcompiler.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS > ov30.log 2>&1
echo OV30=%errorlevel%
type ov30.log
