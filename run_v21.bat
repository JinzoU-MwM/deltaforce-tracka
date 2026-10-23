@echo off
cd /d C:\Users\User\trackA
taskkill /IM dfoverlay19.exe /F >nul 2>&1
taskkill /IM menu_imgui.exe /F >nul 2>&1
taskkill /IM dfimgui_overlay.exe /F >nul 2>&1
schtasks /create /tn dfa_c19 /tr "C:\Users\User\trackA\dfsecinject.exe 33172 C:\Users\User\trackA\dfcore19.dll" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_c19
ping -n 3 127.0.0.1 >nul
schtasks /delete /tn dfa_c19 /f >nul
schtasks /create /tn dfa_oi /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfimgui_overlay.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_oi
ping -n 3 127.0.0.1 >nul
schtasks /delete /tn dfa_oi /f >nul
tasklist | findstr /I dfimgui
