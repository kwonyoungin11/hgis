ka-hgis portable package
========================
This folder includes ka-hgis.exe + data/docs scripts.

REQUIRED on every PC:
  OSGeo4W with qgis-dev / Qt6 / gdal-dev / pdal-dev
  Default root: D:\OSGeo4W  (or set OSGEO4W_ROOT)

Run:
  run.bat

Note:
  QGIS/Qt DLLs are NOT bundled (hundreds of MB + license complexity).
  Install OSGeo4W on the other PC, then run.bat works.
  For full source development: git clone + scripts\build-all.ps1
