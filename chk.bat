@echo off
tasklist /FI "IMAGENAME eq DeltaForceClient-Win64-Shipping.exe"
dir dfsecinject.exe dfoff2.dll 2>nul
