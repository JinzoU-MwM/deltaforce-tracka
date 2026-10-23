@echo off
cd /d C:\Users\User\trackA
del inj_early.log 2>nul
:wait
set "PID="
for /f "tokens=2 delims=," %%p in ('tasklist /FI "IMAGENAME eq DeltaForceClient-Win64-Shipping.exe" /FO CSV /NH') do set "PID=%%~p"
if not defined PID (
  ping -n 2 127.0.0.1 >nul
  goto wait
)
echo GAME_PID=%PID%>> inj_early.log
dfsecinject.exe %PID% dfcore29.dll >> inj_early.log 2>&1
echo EXIT=%errorlevel%>> inj_early.log
