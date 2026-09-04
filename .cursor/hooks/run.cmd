@echo off
setlocal
if "%~1"=="" (
  echo run.cmd: missing script name 1>&2
  exit /b 1
)
set "HOOK=%~dp0%~1.ps1"
if not exist "%HOOK%" (
  echo run.cmd: missing %HOOK% 1>&2
  exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%HOOK%"
exit /b %ERRORLEVEL%
