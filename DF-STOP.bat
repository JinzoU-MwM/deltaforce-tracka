@echo off
taskkill /IM dfoverlay.exe /F >nul 2>&1
echo overlay stopped
timeout /t 2 >nul
