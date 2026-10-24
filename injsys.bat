@echo off
rem injsys.bat — inject core30 sebagai SYSTEM (schtasks /ru SYSTEM).
rem PID dicari otomatis; binary diambil dari build dir (TRACKA_DIR / %USERPROFILE%\trackA).
rem Pemakaian: injsys.bat [nama_dll]   (default dfcore30.dll)
setlocal EnableDelayedExpansion
set "BIN=%TRACKA_DIR%"
if "%BIN%"=="" set "BIN=%USERPROFILE%\trackA"
if not exist "%BIN%\dfsecinject.exe" if exist "C:\Users\User\trackA\dfsecinject.exe" set "BIN=C:\Users\User\trackA"
cd /d "%BIN%"
set "DLL=dfcore30.dll"
if not "%~1"=="" set "DLL=%~1"
set "IMG=DeltaForceClient-Win64-Shipping.exe"
if not "%DF_IMAGE%"=="" set "IMG=%DF_IMAGE%"
set "LOG=%BIN%\inj_sys.log"
del "%LOG%" >nul 2>&1
echo [%DATE% %TIME%] start injsys bin=%BIN% dll=%DLL%>> "%LOG%"

call "%~dp0find_game_pid.bat" "%IMG%" > nul 2>&1
if errorlevel 1 (
  echo ERROR: game tidak jalan — jalankan ulang setelah DeltaForceClient-Win64-Shipping.exe hidup>> "%LOG%"
  type "%LOG%"
  exit /b 4
)
set "PIDS="
for /f "usebackq skip=1 delims=" %%l in ("%~dp0GAME_PID.txt") do set "PIDS=%%l"

set "RC=0"
for %%p in (%PIDS%) do (
  echo --- pid=%%p>> "%LOG%"
  "%BIN%\dfsecinject.exe" %%p "%DLL%" >> "%LOG%" 2>&1
  set "R=!errorlevel!"
  echo EXIT_pid_%%p=!R!>> "%LOG%"
  if not "!R!"=="0" set "RC=!R!"
)
set "RCF=!RC!"
echo TOTAL_EXIT=!RCF!>> "%LOG%"
type "%LOG%"
endlocal & exit /b %RCF%

