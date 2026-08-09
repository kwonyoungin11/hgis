# 고고학 전용 HGIS (ka-hgis) v0.3

C++/Qt6 독립 실행형 필드고고학 HGIS. **OSGeo4W qgis-dev (QGIS 4.x) 라이브러리 링크**. 소스 포크 아님.

기본 CRS: **EPSG:5179**. 측량 폴리곤 중심 작성(제26조급 규칙 검수).

**저장소:** https://github.com/kwonyoungin11/hgis · 브랜치 `main`

## 다른 PC에서 작업

상세: [`docs/other-pc-setup.md`](docs/other-pc-setup.md)

```powershell
git clone https://github.com/kwonyoungin11/hgis.git
cd hgis
# (최초 1회) 관리자 PowerShell — CMake/VS/OSGeo4W 시도
# .\scripts\install-deps.ps1
$env:PATH = "C:\CMake\bin;" + $env:PATH
.\scripts\build-all.ps1
.\scripts\run-ka-hgis.ps1
```

- **개발:** 클론 + OSGeo4W(`qgis-dev` 등) + VS2022 + CMake → `build-all.ps1`
- **실행만:** `dist\ka-hgis-portable\` 복사 후 `run.bat` (대상 PC에도 OSGeo4W 필요, DLL 미포함)
- 조사 GPKG 등 필드 데이터는 git에 없음 → 별도 복사

## 환경 (검증된 구성)
- CMake 4.4+ (`C:\CMake\bin` 권장)
- VS 2022 BuildTools MSVC 19.4x
- OSGeo4W SSOT: **`C:\OSGeo4W`** (env `OSGEO4W_ROOT` → `C:\OSGeo4W` → `D:\OSGeo4W` 순)
  - 패키지: `qgis-dev`, `qt6-devel`, `gdal-dev-devel`, `sqlite3-devel`, `pdal-dev`
- 산출물: `build\Release\ka-hgis.exe`, `ka_hgis_tests.exe`, `ka_workflow_tests.exe`

## 원클릭 빌드·검증·배포
```powershell
cd <repo>\hgis
$env:PATH = "C:\CMake\bin;" + $env:PATH
.\scripts\build-all.ps1
```
포함: cmake build → ctest → smoke-quit → e2e → `dist\ka-hgis-portable`

## 수동 빌드
```powershell
$env:PATH = "C:\CMake\bin;" + $env:PATH
.\scripts\dev-env.ps1
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DOSGEO4W_ROOT=C:/OSGeo4W -DKA_HGIS_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
.\scripts\run-ka-hgis.ps1 --smoke-quit
.\scripts\e2e-smoke.ps1
```

## 실행
```powershell
.\scripts\run-ka-hgis.ps1
# 또는 포터블 (형제 ka-hgis.exe 우선)
cd dist\ka-hgis-portable
.\run.bat
# 또는
.\run-ka-hgis.ps1
```

### 제품 축 (GOAL-ORIG-1)

1. **새 조사** → GPKG 도메인 5레이어 (조사구역/유구면/유구선/단면선/GPS기준점)
2. **배경지도** 메뉴에서 참조 지도 선택 (OSM/VWorld) — 조사 데이터와 분리 (범례 그룹)
3. **디지타이즈** → 툴바 구역/유구면/선 + **편집저장** 커밋 / **그리기종료**
4. **도면검수** → 체크리스트
5. **제출패키지** → SHP(+PDF) EPSG:5179 + MANIFEST

작업 CRS는 5186/5187 가능. 업로드는 5179 변환.

### 도구 메뉴
| 기능 | 설명 |
|---|---|
| CRS 이름만 지정 | 좌표 변환 없이 라벨만 변경 (위험) |
| 좌표 변환(재투영) | 벡터 → 새 GPKG/SHP |
| 지오레퍼런스 | GCP≥2 월드파일; pixel_x/y 3점+ 시 아핀 LS |
| OSM 배경 | XYZ 타일 |
| 유구 스타일 | kind/period 범주 |

## 문서
- **다른 PC 셋업:** `docs/other-pc-setup.md`
- 시나리오: `docs/user/gui-scenario-checklist.md`
- ADR: `docs/adr/0001-standalone-cpp-qgis-libs.md`
- UX: `docs/ux/ia-beginner.md`
- 데이터 흐름: `docs/architecture/data-flow.md`
- 잡카드: `docs/user/job-cards/`
- 에이전트: `AGENTS.md`, `OPENCODE_HANDOFF.md`

## 런타임 주의
`PATH`에 `qgis-dev\bin`, `Qt6\bin`, `gdal-dev\bin`, **`pdal-dev\bin`** 필요 (`pdal-devcpp210.dll`).  
`scripts\dev-env.ps1` / `run-ka-hgis.ps1`이 설정합니다.

VWorld 배경지도는 **도움말 → VWorld API 키 설정**에 키가 있을 때만 추가됩니다 (바이너리 기본 키 없음).

## 라이선스
GNU GPLv2 or later (QGIS 링크 파생물)

## 커밋 진행 상태 (항상 확인)

현재 브랜치/HEAD/원격 동기화/최근 커밋 목록:

- 파일: `docs/COMMIT_STATUS.md`
- 수동 갱신: `.\scripts\update-commit-status.ps1`
- 훅 설치(한 번): `.\scripts\install-git-hooks.ps1`  → 이후 **매 커밋마다 자동 갱신**
- 헬퍼 커밋: `.\scripts\commit.ps1 -Message "..." -Path path1,path2 -Push`
