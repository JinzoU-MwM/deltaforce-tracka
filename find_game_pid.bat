@echo off
rem find_game_pid.bat — cari PID proses target, tulis ke GAME_PID.txt (baris1=count, baris2=pid,pid,...)
rem Pemakaian: find_game_pid.bat [image.exe]   (default DeltaForceClient-Win64-Shipping.exe)
rem Exit 0 = ketemu, 1 = tidak jalan. Tidak ada PID hardcode.
setlocal EnableDelayedExpansion
set "IMAGE=DeltaForceClient-Win64-Shipping.exe"
if not "%~1"=="" set "IMAGE=%~1"
set "OUT=%~dp0GAME_PID.txt"
set "COUNT=0"
set "PIDS="
for /f "tokens=2 delims=," %%p in ('tasklist /FI "IMAGENAME eq %IMAGE%" /FO CSV /NH 2^>nul') do (
  set "P=%%~p"
  if not "!P!"=="INFO:" if not "!P!"=="" if not "!P!"=="0" (
    echo !P!| findstr /r "^[0-9][0-9]*$" >nul && (
      set /a COUNT+=1
      if "!PIDS!"=="" (set "PIDS=!P!") else (set "PIDS=!PIDS!,!P!")
    )
  )
)
if "!COUNT!"=="0" (
  > "%OUT%" echo 0
  >> "%OUT%" echo NOT_FOUND
  echo NOT_FOUND image=%IMAGE%
  endlocal & exit /b 1
)
> "%OUT%" echo !COUNT!
>> "%OUT%" echo !PIDS!
echo COUNT=!COUNT! PIDS=!PIDS!
endlocal & exit /b 0
