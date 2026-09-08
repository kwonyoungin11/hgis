@echo off
setlocal
echo [ka-hgis] Running Auto Git Push to backup/auto-save...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0auto-git-push.ps1"
echo [ka-hgis] Done.
pause
