@echo off
setlocal
set "CAPCOM_ROOT=%~dp0"
set "CAPCOM_EXE=%CAPCOM_ROOT%cli\build\debug\cap.exe"
if not exist "%CAPCOM_EXE%" (
  echo CapCom CLI not found: "%CAPCOM_EXE%"
  exit /b 1
)
"%CAPCOM_EXE%" %*
exit /b %ERRORLEVEL%
