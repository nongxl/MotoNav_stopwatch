@echo off
chcp 65001 >nul
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Build-Countdown.ps1"
if errorlevel 1 echo Build failed. See the error above.
pause
