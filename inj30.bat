@echo off
cd /d C:\Users\User\trackA
del inj30.log 2>nul
dfsecinject.exe 35656 dfcore30.dll > inj30.log 2>&1
echo EXIT=%errorlevel% >> inj30.log
