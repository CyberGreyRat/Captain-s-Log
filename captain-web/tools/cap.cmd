@echo off
if /I "%~1"=="gui" (
  shift
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\Users\luec-a\Documents\CapCom\captain-web\cap-gui.ps1" %*
  exit /b %errorlevel%
)
"%~dp0cap.exe" %*
