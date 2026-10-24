@echo off
rem inj30.bat — inject dfcore30.dll ke game yang SUDAH jalan. PID dicari otomatis.
rem Jalankan sebagai SYSTEM (schtasks) agar MapViewOfFile tidak 0xC0000022.
rem Arg1 = nama dll (default dfcore30.dll). Lokasi exe/dll dari build dir (TRACKA_DIR
rem atau ~\trackA); repo ini tidak menyimpan binary.
setlocal EnableDelayedExpansion
set "BIN=%TRACKA_DIR%"
if "%BIN%"=="" set "BIN=%USERPROFILE%\trackA"
if not exist "%BIN%\dfsecinject.exe" if exist "C:\Users\User\trackA\dfsecinject.exe" set "BIN=C:\Users\User\trackA"
cd /d "%BIN%"
set "DLL=dfcore30.dll"
if not "%~1"=="" set "DLL=%~1"
set "IMG=DeltaForceClient-Win64-Shipping.exe"
if not "%DF_IMAGE%"=="" set "IMG=%DF_IMAGE%"
set "LOG=%BIN%\inj30.log"
del "%LOG%" >nul 2>&1

if not exist "%BIN%\dfsecinject.exe" (
  echo ERROR: dfsecinject.exe tidak ada di %BIN%>> "%LOG%"
  echo Set TRACKA_DIR ke folder build host.
  type "%LOG%"
  exit /b 2
)
if not exist "%BIN%\%DLL%" (
  echo ERROR: %DLL% tidak ada di %BIN%>> "%LOG%"
  type "%LOG%"
  exit /b 3
)

call "%~dp0find_game_pid.bat" "%IMG%" > nul 2>&1
if errorlevel 1 (
  echo ERROR: %IMG% tidak jalan>> "%LOG%"
  type "%LOG%"
  exit /b 4
)
set /p COUNT=<"%~dp0GAME_PID.txt"
set "PIDS="
for /f "usebackq skip=1 delims=" %%l in ("%~dp0GAME_PID.txt") do set "PIDS=%%l"
if "%PIDS%"=="" (
  echo ERROR: PID kosong>> "%LOG%"
  exit /b 4
)
echo [%DATE% %TIME%] bin=%BIN% dll=%DLL% count=%COUNT% pids=%PIDS%>> "%LOG%"

set "RC=0"
for %%p in (%PIDS%) do (
  echo --- inject pid=%%p>> "%LOG%"
  "%BIN%\dfsecinject.exe" %%p "%DLL%" >> "%LOG%" 2>&1
  set "R=!errorlevel!"
  echo EXIT_pid_%%p=!R!>> "%LOG%"
  if not "!R!"=="0" set "RC=!R!"
)
set "RCF=!RC!"
echo TOTAL_EXIT=!RCF!>> "%LOG%"
type "%LOG%"
endlocal & exit /b %RCF%

