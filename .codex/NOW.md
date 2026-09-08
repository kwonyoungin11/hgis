# NOW — Codex Resume

## Current State (2026-09-08)

### 고고학 전용화 1단계 — 검증 미완료 (2026-09-08)

- 최신 사용자 요청은 1 우클릭 메뉴 → 2 UI/동선/Undo → 3 격자 → 4 조판 → 5 전체 GIS 정리이며, 각 단계 후 멈춰 보고한다. 이전 Phase 계획보다 이 요청을 우선한다. **현재 1단계만 구현 중이고 2~5단계는 완료하지 않았다.**
- 새 `MainWindowContextMenus.cpp`에서 layer_key/명시적 역할/공급자로 메뉴를 나눈다. 불필요 항목은 제외, 일시적으로 불가능한 명령은 비활성+한국어 툴팁, 삭제는 마지막 그룹. 우클릭 행과 작업 대상을 일치시키고 목록 제거와 원본 도형 삭제를 분리했다.
- 선택 도구 메뉴도 조사 layer_key에만 편집 항목을 제공한다. 이전/다음 지도 범위는 QGIS 기존 기능을 사용한다. 가져온 래스터/CAD에는 imported_reference 표식을 보존하며 정합 결과 객체에도 승계한다.
- 작업 전 소스 사본/합성 9종 fixture/변경 전 캡처 8개/단계 전용 패치: `build/qa/archaeology-step1/`. 상세 상태는 같은 폴더 `REPORT.md`.
- **사용자가 물리 Esc로 Computer Use를 중단했다. 이 턴에서는 추가 UI 조작을 하지 않는다.** 지적 WMS 메뉴는 관찰했지만 파일 저장 전에 중단됐고 지도 캔버스 및 변경 후 9종 portable 캡처는 남았다.
- 현재 사용자가 `build/Release/ka-hgis.exe`에서 실제 조사를 열어 파일이 잠겨 있다. 최종 링크 LNK1104가 발생했으며 사용자 앱을 종료하지 않았다. 테스트 실패를 별도 진단 중이다. 최신 단계의 스모크/배포/화면 확인은 통과했다고 보고하지 않는다.
- Ctrl+Z/삭제 키 전체, UI 글자·크기·색·여백, 격자 알고리즘과 조판 흐름은 이번 단계 변경에 포함하지 않았다. 저장되지 않은 도형 편집의 Undo/레이어 제거 키 연결은 2단계에서 우선 확인해야 한다. 기존 원본 데이터와 다른 미커밋 변경을 보존하고 커밋하지 않았다.

### 현장 완성도 — Phase 0 / Phase 1 (2026-09-08)

- 사용자는 Phase 0 측정 후 Phase 1 안정화까지 진행하고, 각 Phase 끝에서 멈춰 보고하도록 요청했다. Phase 2~6을 이어서 자동 진행하지 않는다.
- Phase 0은 제품 수정 없이 약 13분 동안 측정했다. 실제 portable 캡처와 24개 참조 레이어/24,000 피처 반복 측정, 작업 전 소스 사본은 `build/qa/production-phase0/REPORT.md`와 같은 폴더에 있다.
- Phase 1: 새 조사 동명 파일 보호, QGZ 원자적 교체·실패 복원, Save As 실패 시 원본 소스/메모리 피처 복원, 닫기 저장의 메모리 벡터 보관, 조사 전환 전 미저장 확인, 저장 중 닫기 재진입 차단을 구현했다. 다른 이름으로 저장은 기존 대상 GPKG/QGZ를 덮어쓰지 않고 새 이름을 요구한다.
- 위치/행정경계 요청은 전체 20초, 참조지도 요청은 각 15초 제한과 취소를 적용했다. 토양·지질·수계 및 고지형의 토양 준비는 QgsTask에서 수행하고 GUI에는 값과 파일만 전달한다. 종료·조사 전환 뒤 늦은 결과는 적용하지 않는다.
- 설치 QGIS 4.3 `QgsArchive::zip`의 공통 `qgis-project-XXXXXX.zip` 경합을 확인했다. 앱은 Qt/QGIS 생성 전 프로세스별 TEMP/TMP를 분리하며, 생성된 래스터의 저장 후 참조를 보존하려고 자동 삭제하지 않는다. CTest도 각 테스트별 임시 폴더를 사용한다.
- 꼭짓점 이동의 저장 실패는 편집 버퍼를 유지하고 한국어로 알린다. 화면 CRS와 레이어 CRS가 다를 때 좌표 변환도 보완했다.
- 최종 Release 빌드, CTest **17/17 (152.32초)**, 시작 스모크, portable 배포가 통과했다. Release/portable SHA256은 `B4FC088F23430D42B248C04060FCC80EFF6321FDB8326F205E6587D0ECF24FBC`로 일치한다. 실제 portable 화면 확인과 재측정 결과는 Phase 1 보고서에 마무리한다.
- Phase 1 산출물: `build/qa/production-phase1/`. 기존 미커밋 변경과 원본 조사 자료를 보존했다. 커밋하지 않았다.

### New survey work CRS verification (2026-09-08)

- Fixed the stale work-CRS status chip on new survey creation and saved-project reopen. The chip now follows the canvas destination CRS change signal; a selected CRS is committed to the session only after survey creation succeeds.
- Verified both EPSG:5186 and EPSG:5187 through the real portable application's new-survey dialog, save, and explicit reopen. Screenshots: `build/qa/crs-selection/{created,reopened}-{5186,5187}.jpg`.
- Regression tests assert project CRS, canvas CRS, GPKG layer CRS, manual status-chip switching, reopen, and the invariant submission chip 5179.
- Release build and isolated smoke passed (exit 0). Final CTest: 14/14 passed, exit 0, 167.16 seconds. Corrected two existing test fixtures (hidden canvas output-size initialization; actual polygon required for composed layout) without relaxing their checks or changing product GIS/layout behavior.
- Published portable executable matches the Release SHA256. Original field data and the geology 80 km / river 160 km ranges were not changed. No commit.

The current requested startup behavior is **home screen only**. On launch, ka-hgis opens the application shell and waits for the user to choose **조사 열기**, a recent survey item, or **새 조사**.

Automatic startup restore is intentionally disabled. Do not reintroduce automatic LayersOnly restore, last-project restore, drawing restore, basemap restore, WMS/XYZ restore, or background project loading on app startup.

The previous automatic restore path loaded 31 layers, including a 390k-lot cadastral layer, and missed the aerial imagery. The measured startup/log delay included survey opening work that should happen only after the user explicitly opens a survey.

Current implementation direction:

- Constructor-time map/background initialization and scheduled last-survey restore were removed or disabled by default.
- Explicit open paths remain valid: **조사 열기**, recent survey click, and **새 조사**.
- Do not modify original survey data during startup.
- Do not commit unless the user explicitly asks.

Validation expected for future C++ startup/UI work:

- configure/build Release with the repo PowerShell commands;
- run `ctest --test-dir build -C Release --output-on-failure`;
- run `.\scripts\run-ka-hgis.ps1 --smoke-quit`;
- run `.\scripts\publish-desktop.ps1` for field-facing UI changes.

Docs/settings-only work does not require rebuilding the C++ application.

## Recent Product Notes To Preserve

- 안동 뷰어 work is separate from the main ka-hgis product. Do not fold it into `MainWindow` unless explicitly requested.
- VWorld keys remain local only. Do not hardcode a production key.
- `removeAllMapLayers()` remains forbidden in survey-load flows.
- Work CRS may be EPSG:5186 or EPSG:5187; upload/export output stays EPSG:5179 SHP + PDF + MANIFEST.
- Domain layers appear in the legend only after draw/import/open of actual user data.
- Reference maps stay separate from survey data.

## Harness Migration

This project now treats `AGENTS.md`, `.codex/NOW.md`, `HANDOFF.md`, and `docs/HANDOFF.md` as the active Codex-native guidance surface.

Legacy `.grok`, `.cursor`, `.agents` dispatch/history files, and Orca files may remain for history or compatibility, but they are not the active preset source for new Codex work. The project-local `.agents/skills/ka-hgis-gis/SKILL.md` is the maintained Codex GIS specialization. Do not add new project-level Grok, Antigravity, Cursor-only, or old fixed model routing requirements unless the user explicitly asks for that compatibility layer.

## Project-local GIS setup (2026-09-08)

General C++/GPT-6 settings remain global. HGIS GIS instructions and the `ka-hgis-gis` skill live only in this repository. The skill links GIS evidence, source areas and relevant regression tests. Historical C++17/LTR and 5179 schema notes are distinguished from the current toolchain and survey CRS selection. This is a documentation/skill configuration change, not a product behavior change or new application test run.
