@echo off
rem watch_inject.bat — tunggu game start, lalu inject SEKALI (awal = paling andal vs ACE).
rem Via schtasks SYSTEM. Arg1 = dll (default dfcore30.dll), Arg2 = timeout detik (0 = tanpa batas).
setlocal EnableDelayedExpansion
set "BIN=%TRACKA_DIR%"
if "%BIN%"=="" set "BIN=%USERPROFILE%\trackA"
if not exist "%BIN%\dfsecinject.exe" if exist "C:\Users\User\trackA\dfsecinject.exe" set "BIN=C:\Users\User\trackA"
cd /d "%BIN%"
set "DLL=dfcore30.dll"
if not "%~1"=="" set "DLL=%~1"
set "LIMIT=0"
if not "%~2"=="" set "LIMIT=%~2"
set "IMG=DeltaForceClient-Win64-Shipping.exe"
if not "%DF_IMAGE%"=="" set "IMG=%DF_IMAGE%"
set "LOG=%BIN%\inj_early.log"
del "%LOG%" >nul 2>&1
set /a ELAPSED=0

:wait
call "%~dp0find_game_pid.bat" "%IMG%" > nul 2>&1
if not errorlevel 1 goto found
if not "%LIMIT%"=="0" (
  set /a ELAPSED+=2
  if !ELAPSED! GEQ %LIMIT% (
    echo TIMEOUT %LIMIT%s — game tidak start>> "%LOG%"
    type "%LOG%"
    endlocal & exit /b 5
  )
)
ping -n 3 127.0.0.1 >nul
goto wait

:found
set "PIDS="
for /f "usebackq skip=1 delims=" %%l in ("%~dp0GAME_PID.txt") do set "PIDS=%%l"
echo FOUND bin=%BIN% pids=%PIDS% dll=%DLL%>> "%LOG%"
set "RC=0"
for %%p in (%PIDS%) do (
  "%BIN%\dfsecinject.exe" %%p "%DLL%" >> "%LOG%" 2>&1
  set "R=!errorlevel!"
  echo EXIT_pid_%%p=!R!>> "%LOG%"
  if not "!R!"=="0" set "RC=!R!"
)
echo DONE rc=!RC!>> "%LOG%"
type "%LOG%"
endlocal & exit /b %RC%
