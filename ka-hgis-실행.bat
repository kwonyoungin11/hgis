@echo off
REM Console-free launch. Do not run ka-hgis.exe directly (OSGeo DLL path missing).
cd /d "%~dp0"
start "" wscript.exe //nologo "%~dp0scripts\start-ka-hgis.vbs"
exit /b 0
