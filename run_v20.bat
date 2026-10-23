@echo off
cd /d C:\Users\User\trackA
taskkill /IM menu_imgui.exe /F >nul 2>&1
taskkill /IM dfoverlay19.exe /F >nul 2>&1
schtasks /create /tn dfa_menu /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\menu_imgui.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_menu
ping -n 3 127.0.0.1 >nul
schtasks /delete /tn dfa_menu /f >nul
schtasks /create /tn dfa_o19 /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\dfoverlay19.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_o19
ping -n 3 127.0.0.1 >nul
schtasks /delete /tn dfa_o19 /f >nul
tasklist | findstr /I "menu_imgui dfoverlay"
