# 고고학 전용 HGIS (ka-hgis)

C++/Qt6 독립 실행형 필드고고학 HGIS. **OSGeo4W qgis-dev (QGIS 4.x) 라이브러리 링크**. 소스 포크 아님.

## 해결된 빌드 환경 (이 머신 검증)
- CMake 4.4.1
- VS 2022 BuildTools MSVC 19.44
- OSGeo4W root: `D:\OSGeo4W`
- Packages: `qgis-dev`, `qt6-devel`, `gdal-dev-devel`, `sqlite3-devel`, ...
- 산출물: `build\Release\ka-hgis.exe` (smoke-quit=0), `ka_hgis_tests.exe` (ctest PASS)

## 빌드
```powershell
cd D:\qgis
# VS x64 개발자 환경 + PATH에 CMake
$env:OSGEO4W_ROOT="D:\OSGeo4W"
.\scripts\dev-env.ps1
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DKA_HGIS_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\scripts\run-ka-hgis.ps1 --smoke-quit
.\scripts\e2e-smoke.ps1
```

## 실행
```powershell
.\scripts\run-ka-hgis.ps1
```
7단계: 새 조사 → 배경 → 구역 → 유구 → GPS → 검수 → 제출

## 문서
- ADR: docs/adr/0001-standalone-cpp-qgis-libs.md
- UX: docs/ux/ia-beginner.md
- 연구: docs/research/
- 잡카드: docs/user/job-cards/

## 라이선스
GNU GPLv2 or later (QGIS 링크 파생물)
