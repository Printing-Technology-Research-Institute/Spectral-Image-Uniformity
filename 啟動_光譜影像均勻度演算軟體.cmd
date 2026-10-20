@echo off
setlocal EnableExtensions
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0prepare_fx17_runtime.ps1"
if not errorlevel 1 exit /b 0
echo Startup failed. Please keep the entire portable folder and check permissions.
pause
exit /b 1
