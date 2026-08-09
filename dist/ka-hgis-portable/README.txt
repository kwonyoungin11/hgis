ka-hgis portable (DLLs not vendored)

REQUIREMENTS on this PC:
  - OSGeo4W with qgis-dev (recommended root: C:\OSGeo4W)
  - Also accepts OSGEO4W_ROOT env, then D:\OSGeo4W

RUN:
  run.bat
  or: powershell -File .\run-ka-hgis.ps1

Launcher uses ka-hgis.exe in THIS folder first, then sets PATH via dev-env.ps1.

Full second-PC guide (clone + build): see repo docs/other-pc-setup.md
  https://github.com/kwonyoungin11/hgis
