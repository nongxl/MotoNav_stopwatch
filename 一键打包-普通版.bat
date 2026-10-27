@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Build-Personal.ps1"
if errorlevel 1 echo Build failed. See the error above.
pause
