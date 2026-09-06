@echo off
setlocal
cd /d "%~dp0\.."
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\tools\run-qemu.ps1"
if errorlevel 1 (
  echo.
  echo QEMU nie zostal uruchomiony. Przewin terminal wyzej do pierwszego bledu.
  pause
)
