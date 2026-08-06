# Windows 빌드

1. OSGeo4W 설치, 패키지 `qgis-ltr-dev` 및 Qt5
2. MSVC 툴셋이 QGIS 빌드와 맞는지 확인 (불일치 시 링크/런타임 실패)
3. `scripts/dev-env.ps1` 실행 후 cmake
4. 성공 시 `VERSION_QGIS_PIN.txt`에 실제 QGIS 버전 기록

환경변수: OSGEO4W_ROOT, QGIS_PREFIX_PATH, GDAL_DATA, PROJ_LIB, QT_PLUGIN_PATH
