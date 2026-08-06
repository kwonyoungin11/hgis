# 고고학 전용 HGIS (ka-hgis) v0.3

C++/Qt6 독립 실행형 필드고고학 HGIS. **OSGeo4W qgis-dev (QGIS 4.x) 라이브러리 링크**. 소스 포크 아님.

기본 CRS: **EPSG:5179**. 초보자 7단계 UI. 측량 폴리곤 중심 작성(제26조급 규칙 검수).

## 환경 (이 머신 검증)
- CMake 4.4+, VS 2022 BuildTools MSVC 19.4x
- OSGeo4W: `D:\OSGeo4W` — `qgis-dev`, `qt6-devel`, `gdal-dev-devel`, `sqlite3-devel`, `pdal-dev`
- 산출물: `build\Release\ka-hgis.exe`, `ka_hgis_tests.exe`, `ka_workflow_tests.exe`

## 원클릭 빌드·검증·배포
```powershell
cd D:\qgis
$env:OSGEO4W_ROOT="D:\OSGeo4W"
.\scripts\build-all.ps1
```
포함: cmake build → ctest → smoke-quit → e2e → `dist\ka-hgis-portable`

## 수동 빌드
```powershell
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
# 또는 포터블
cd dist\ka-hgis-portable
.\run.ps1
```

### 7단계
1. 새 조사 (GPKG 도메인 레이어)
2. 배경 (파일 / OSM)
3. 구역 디지타이즈
4. 유구 폴리곤 (kind/period 필수)
5. GPS/GCP (accuracy_m, pdop, fix_type, pixel_x/y)
6. 검수 (18규칙 체크리스트)
7. 제출 (PDF 5종 + SHP 패키지 + MANIFEST.sha256)

### 도구 메뉴
| 기능 | 설명 |
|---|---|
| CRS 이름만 지정 | 좌표 변환 없이 라벨만 변경 (위험) |
| 좌표 변환(재투영) | 벡터 → 새 GPKG/SHP |
| 지오레퍼런스 | GCP≥2 월드파일; pixel_x/y 3점+ 시 아핀 LS |
| OSM 배경 | XYZ 타일 |
| 유구 스타일 | kind/period 범주 |

## 문서
- 시나리오: `docs/user/gui-scenario-checklist.md`
- ADR: `docs/adr/0001-standalone-cpp-qgis-libs.md`
- UX: `docs/ux/ia-beginner.md`
- 데이터 흐름: `docs/architecture/data-flow.md`
- 잡카드: `docs/user/job-cards/`

## 런타임 주의
`PATH`에 `qgis-dev\bin`, `Qt6\bin`, `gdal-dev\bin`, **`pdal-dev\bin`** 필요 (`pdal-devcpp210.dll`).  
`scripts\dev-env.ps1` / `run-ka-hgis.ps1`이 설정합니다.

## 라이선스
GNU GPLv2 or later (QGIS 링크 파생물)
