@echo off
setlocal
REM Resolve scripts next to this dispatcher. Do not put GROK_WORKSPACE_ROOT
REM in hook JSON commands: Grok expands that token to empty on Windows.
if "%~1"=="" (
  echo run.cmd: missing script name 1>&2
  exit /b 1
)
set "HOOK=%~dp0bin\%~1.ps1"
if not exist "%HOOK%" (
  echo run.cmd: missing %HOOK% 1>&2
  exit /b 1
)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%HOOK%"
exit /b %ERRORLEVEL%
