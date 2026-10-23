@echo off
cd /d C:\Users\User\trackA
taskkill /IM dfimgui_overlay.exe /F >nul 2>&1
ping -n 2 127.0.0.1 >nul
copy /Y dfimgui_overlay_multi.exe dfimgui_overlay.exe >nul
start "" dfimgui_overlay.exe
