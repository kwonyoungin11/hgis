# NOW — Grok resume (2026-08-22)

Cursor에서 Grok Build로 이 대화를 이을 때 **이 파일을 먼저** 읽는다.
커밋·리셋·force-push 금지. 기존 미커밋 파일은 절대 초기화하지 않는다.

## 이 PC

- Repo: `A:\hgis - 복사본` · branch는 로컬 작업본
- OSGeo: **`A:\OSGeo4W`** (`scripts/dev-env.ps1`이 C/D 없으면 A를 씀)
- CMake: `C:\CMake\bin`
- 실행: `.\scripts\run-ka-hgis.ps1` (exe를 직접 켜지 말 것)
- 답변: 한국어. UI 문자열: 한국어. 식별자: 영어

## 지금 (2026-08-24 Orca)

작업 트리 설정: `orca.yaml` `scripts.setup` → `scripts/orca-worktree-setup.ps1` (OSGeo + CMake, build/ 공유 금지).
Orca 프로젝트 훅: 기본적으로 실행, agent 전에 설정 완료 대기 (`wait-for-setup`).
커밋 금지.

## 지금 (2026-08-24 최적화)

단면도 맵: `zoomToExtent`만 ( `setExtent` 제거 ). 빈 GeoTIFF는 `ka_section_blank` 메모리 폴리곤 — `setLayers({})` 금지. 축척자 눈금 잉크 `#111827` 0.30 mm. QSS 메인툴바 구분선·emptyState 점선 웰.
되돌리기: 이전 백업 `refs/backup/pre-uiux-opt-20260824` 또는 `.unlazy\uiux-opt\restore.ps1`
바탕화면 아이콘으로 빈 단면도(눈금만, 위성 없음) 확인. 커밋 금지.

## 지금 (2026-08-24 UI/UX 패스)

되돌리기: `powershell -File .unlazy\uiux-opt\restore.ps1` 또는 `git restore --source=refs/backup/pre-uiux-opt-20260824 -- data/theme src/app src/core tests`
기능 유지. 바뀐 것: 축척자 잉크 `#111827/#FFFFFF`, 단면 빈 GeoTIFF 안내, 표고 격자 `#374151`, 조판 열 때 좌표점 자동 시작 제거, QSS sampleTile:checked.
바탕화면 **고고학 전용 HGIS**로 확인. 커밋 금지.

## 지금 (2026-08-24 감사)

`/unlazy` 완료 여부: **미커밋 조각은 코드상 닫힘, 제품 백로그는 열림.**
원장 `.unlazy/ka-hgis/GATES.md` — 6 met / 1 abandoned (G6 바탕화면 클릭) / 0 unmet.
열린 것: `layout_exists` 빈 자동조판, `SectionLayoutService` `setExtent` after `zoomToExtent`, 빈 `setLayers({})`, 커밋은 사용자 지시 후.
테스트 구멍: 표고 보정·원본 `setCrs`/`setDpi` 금지·PDF 300/벡터·미리보기 Nearest·GeoTIFF 추가 경로. 색: 평면도 축척자 fill 미잠금, 단면 눈금 스타일 위성에서 증발, 표고 격자 `#888888`.

## 지금 (2026-08-23)

- 전문가팀 상시: `arcgis-expert` ∥ `qgis-expert` then `ka-developer` + 디자이너 4. 훅 `.grok/hooks/experts-team.json`. TDD `32-tdd.md`. 사전설정 `21-predev.md`.
- 단면 축척자: 샘플 4개 제거, 교호식 Double Box 하나 (`ka_section_scale_bar`), Fixed 세그먼트, 지도 아래 좌측.

## 단면도 — 완료 (다시 만들지 말 것)

Bentley Descartes 단면 GeoTIFF(X=거리, Y=표고) → A3/A4 가로 조판 → 벡터 PDF.

| 구역 | 파일 | 상태 |
| --- | --- | --- |
| 눈금/조판/PDF | `src/core/SectionLayoutService.*` | 통과. 프레임 종횡비=extent, `zoomToExtent` 후 축 정렬 |
| 3열 탭 | `src/app/KaSectionDrawingStudio.*` | 용지+눈금부터. GeoTIFF 추가 버튼. 위성·지적 제외. CRS 5187/5186 |
| 메인 연결 | `MainWindow.*` `KaIcons.cpp` | **단면도** 아이콘, `addSectionGeoTiffFromPath` (범례·줌 없음) |
| 테스트 | `tests/test_section_layout.cpp` `tests/test_section_studio.cpp` | `section_layout_engine` + `section_studio_engine` PASS |
| 설계 | `docs/superpowers/specs/2026-08-22-section-layout-studio-design.md` | 확정 |

검증 증거(2026-08-22): Release 빌드 OK · 위 2테스트 PASS · `--smoke-quit` 0.
`workflow_engine` FAIL은 VWorld `INVALID_KEY` — **기존 실패, 고치지 말 것**.

## 깨면 안 되는 함정

1. `QgsLayoutViewToolSelect::setLayout`은 layout이 붙은 **뒤**에만. `setTool`을 그 전에 호출하면 `QGraphicsScene::addItem`에서 `0xc0000005`.
2. `setLayout(nullptr)` 금지. detach는 `unsetTool` + `setScene(nullptr)`만.
3. 고정 390×261mm 프레임에 `zoomToExtent`만 하면 Y extent가 늘어나고 표고 눈금이 어긋난다. 프레임 종횡비 = GeoTIFF extent.
4. 좌표계 콤보는 5187/5186 **표제만**. 원본 GeoTIFF에 `setCrs` 하지 말 것.
5. 조판은 픽셀 평면(가로=거리, 세로=해발). 세계 4×4는 위치만. 원본에 `setCrs`/`setDpi` 금지.
6. 좌측 목록에 위성·지적·조사 벡터를 넣지 말 것. 탭을 열면 용지+눈금이 있어야 한다.
7. `section_line` 스키마/GPKG 마이그레이션 금지.
8. **덮어쓰기 금지:** `KaDrawingStudio.cpp`, `LayoutService.cpp`, `tests/test_workflow.cpp`의 기존 미커밋 DPI/워크플로 수정.

## 해발 조사 (2026-08-22 22:55, 사용자 정정)

사용자 맞음: **해발이 없는 게 아니라 세계 XYZ로 잘못 읽음.**
입력은 Descartes **단면 평면 사진실측**(벽면을 평면처럼 찍은 직교 사진). 도면 축은 XYZ가 아님.

- 픽셀 가로 = 거리, 픽셀 세로 = 해발 축. 픽셀 ≈ 1 mm가 실측 축척.
- `강릉.tif` 4×4의 194885/574017은 **이 평면이 지도 어디에 놓였는지**만. 도면 세로축이 아님.
- 사진 세로 길이 = 1653×1 mm ≈ **1.65 m** (벽 높이 실측). 이건 파일에 있음.
- `inspectRaster`가 지도 Y(574016)를 해발로 보다가 거부하고 `elevBottom=0` → 눈금 0~1.65 m.
- 4×4 Z행은 0이라 **해발 원점**(하단이 몇 m인지)은 태그에서 안 나옴. 우측 **표고 보정**이 그 원점.
- 34264 Z만 읽는 패치 금지. 원본 `setCrs`/`setDpi` 금지.
- 수평 수정: 세계 4×4는 버리고 픽셀을 거리×해발로 다시 씀. 조판 CRS는 로컬 단면 평면. 5186/5187은 표제만. 표고 보정은 래스터+눈금 같이 이동.

## 다음 (사용자가 시키기 전엔 구현하지 말 것)

1. 테스트 주석 `??` UTF-8 복구 (기능과 무관)
2. A4·지정 축척에서 표제/축척자 겹침이 보이면 chrome 간격만 최소 수정
3. 커밋은 **사용자가 말한 뒤에만**. `.\scripts\commit.ps1` 사용

## 명령

```powershell
$env:PATH = "C:\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1
cmake --build build --config Release
ctest --test-dir build -C Release -R "section_(layout|studio)_engine" --output-on-failure
.\scripts\run-ka-hgis.ps1
```
