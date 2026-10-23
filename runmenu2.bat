@echo off
cd /d C:\Users\User\trackA
taskkill /IM menu_imgui.exe /F >nul 2>&1
del menu.log 2>nul
schtasks /create /tn dfa_menu /tr "C:\Users\User\trackA\sessrun.exe C:\Users\User\trackA\menu_imgui.exe" /sc once /st 00:00 /ru SYSTEM /rl HIGHEST /f >nul
schtasks /run /tn dfa_menu
ping -n 5 127.0.0.1 >nul
schtasks /delete /tn dfa_menu /f >nul
type menu.log
