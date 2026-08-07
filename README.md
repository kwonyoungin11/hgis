# 고고학 전용 HGIS (ka-hgis) v0.3

C++/Qt6 독립 실행형 필드고고학 HGIS. **OSGeo4W qgis-dev (QGIS 4.x) 라이브러리 링크**. 소스 포크 아님.

**작업 CRS:** EPSG:5186(중부) / 5187(동부). **업로드/인트라넷:** EPSG:5179.  
초보자 7단계 UI. 측량 폴리곤 중심 작성(제26조급 규칙 검수).

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
1. 새 조사 (GPKG, 작업 CRS 5186/5187 선택)
2. 배경·지적 (VWorld/파일; 위치검색)
3. 구역 디지타이즈 (폴리곤)
4. 유구 면/선 (kind/period 필수)
5. GPS/GCP ≥2 (accuracy_m, pdop, fix_type 등)
6. 검수 (체크리스트 error=0)
7. 제출: **5179 SHP 변환** + PDF(범례·축척) + 패키지·MANIFEST

### 좌표계·도구
| 기능 | 설명 |
|---|---|
| 작업 CRS 5186/5187 | 수치지형·지적·작도 |
| 5179 SHP 변환 | 문화재 인트라넷 업/다운로드용 |
| CRS 이름만 지정 | 좌표 미변환 라벨만 (위험) |
| 지오레퍼런스 | GCP≥2 월드파일 |
| VWorld/위성 배경 | 한국 타일 배경 |

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

## 커밋 진행 상태 (항상 확인)

현재 브랜치/HEAD/원격 동기화/최근 커밋 목록:

- 파일: `docs/COMMIT_STATUS.md`
- 수동 갱신: `.\scripts\update-commit-status.ps1`
- 훅 설치(한 번): `.\scripts\install-git-hooks.ps1`  → 이후 **매 커밋마다 자동 갱신**
- 헬퍼 커밋: `.\scripts\commit.ps1 -Message "..." -Path path1,path2 -Push`

