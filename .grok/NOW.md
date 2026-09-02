# NOW — Grok resume (2026-08-22)

## 지금 (2026-09-02 맞추기 줌 화살표 + 먹선)

줌 안 하면 번호·화살표 정상. 오른쪽 지적만 줌/팬하면 화면 좌표 캐시가 지적에서 떨어짐. 이동은 mapX/mapY라 사진은 맞음.
고침: 화살표는 `mapToPixel`+`viewport()->mapTo`(QgsMapMouseEvent와 같음). `mapFromScene`은 빼 둠(오른쪽 끝으로 감). 오른쪽 클릭은 힌트 말고 캔버스 `mapPoint`. 왼쪽 그림 `viewChanged`·CAD 줌·DPI에도 다시 찍음.
이동 후 선이 연한 이유: 흰 종이 투명 + opacity 0.82 + 흰색 허용 20이 연한 먹선까지 지움.
고침: opacity 1.0, 흰 여백만 허용 8, Multiply, 대비↑·밝기↓, Nearest.
검증: cmake 0 · georef_engine PASS · smoke 0 · publish 2192384.
필드: 옛 창 닫고 아이콘 → 맞추기 → 점 찍고 오른쪽 줌/팬(화살표가 점에 붙어 있어야 함) → 이동(먹선이 지적 위에서 진해야 함). 커밋 금지. PR 없음.

## 지금 (2026-09-02 맞추기 사진 안 붙음)

멈춤 고치며 in-place persist만 쓰다 JPG가 지적에 안 붙음. 화면은 점 3개·분할은 되는데 이동 후 그림이 좌표에 안 앉음.
고침: 월드파일 + GDAL PAM GT 찍기. persist 실패 시에만 같은 파일 재오픈(refreshAllLayers 없음). 지적 그리는 중이면 350ms 후 한 번 더 그림.
검증: cmake 0 · georef_engine PASS · jpeg 5187 PASS · smoke 0 · publish 0.
필드: 옛 창 닫고 아이콘 → 맞추기 → 점 2개 이상 → 이동. 커밋 금지. PR 없음(autopilot 해당 없음).

## 지금 (2026-09-02 툴바 조합 로딩 최적화)

지형맵·DEM·토양·지질·수계·검색·레이어순서·줌을 잇달아 누르면 `refreshAllLayers`가 지적 WMS 캐시를 버리고 멈춤.
고침: `zoomToLayerMax`/`zoomToKorea`/`setLayerOpacity`/`prepareFieldBasemapPack`/검색/되돌리기/레이어순서는 `refreshCanvasIfIdle`만. 그리는 중이면 skip.
검증: cmake 0 · uiComboActions + zoomToLayerMax + zoomToKorea + refreshXyz QTest PASS · smoke · publish.
필드: 옛 창 닫고 아이콘 → 위성·지적 켠 뒤 툴바를 여러 개 잇달아 켜기. 커밋 금지.

## 지금 (2026-09-02 맞추기 이동 멈춤)

사진을 **열 때가 아님**. 점 짝을 찍고 **이동**할 때 로딩되며 멈춤.
원인: `applyMove`가 월드파일 적용 후 같은 JPG를 `removeMapLayer`+`QgsRasterLayer` 재생성하고, `zoomToLayerMax`/`refreshAllLayers`가 지적 WMS+큰 JPEG를 UI 스레드에서 다시 그림.
고침: `persistAlignedRaster` (같은 레이어, PAM sidecar 제거, 재생성 없음). 이동 후 `refreshCanvasIfIdle`만. 현재 지적 화면 유지.
검증: cmake 0 · georef_engine PASS · smoke-quit 0 · publish-desktop 0 (2191360).
필드: 옛 창 닫고 아이콘 → 맞추기 → 점 2개 이상 → 이동. 커밋 금지.

## 지금 (2026-09-02 범례=레이어 체크)

토양도 두 줄은 맞음: `토양도(흙토람)` 벡터 + `토양도(흙토람 그림)` 산능선 타일. 한 제품.
레이어에서 끄면 범례에서도 빼야 함. 예전에 숨긴 토양도를 범례에 다시 끼워 넣던 버그.
고침: `tuneSheetLegend`는 `isVisible()`만. `syncMapFromLayers`가 범례를 다시 맞춤.
검증: cmake 0 · sheetLegend_followsLayerCheckOnAndOff PASS · smoke · publish.
필드: 아이콘 → 도면만들기 → 레이어 켜고 끄면 범례가 같이 바뀜. 커밋 금지.

## 지금 (2026-09-02 조판 범례)

사용자 스크린샷 범례는 **틀림**. 흙토람 분포지형 색칸이 아니라 `토양도(흙토람 그림)`·지적 본번/부번·위성 **레이어 이름**만 나옴.
고침: `LayoutService::tuneSheetLegend` — Manual 전용 트리에서 그림·WMS/XYZ·지적·위성을 빼고 벡터 `토양도(흙토람)` 분류(산악지·곡간·하성평탄)를 남김. 지도에는 그림이 그대로 깔림.
검증: cmake 0 · sheetLegend QTest PASS · smoke 0 · publish 0.
필드: 옛 창 닫고 아이콘 → 도면만들기 → 범례를 다시 놓기. 커밋 금지.

## 지금 (2026-09-02 토양도 산능선 빈곳)

고지형을 꺼도 산능선이 비는 것은 페이드가 아님. SOIL_1/2/3 벡터에 능선 폴리곤이 없고, 흙토람 웹은 `TOP_A_SOIL_T_GEO` 3857 타일로 산악지를 채운다.
고침: 토양도 내려받기 시 그 그림을 벡터 아래에 깐다. URI crs=3857, zmax=15. 타일 경로는 L00/Rhex/Chex.
검증: cmake 0 · soilTerrainPicture PASS · smoke · publish.
필드: 옛 창 닫고 아이콘 → 토양도 다시 받기. 커밋 금지.

## 지금 (2026-09-02 고지형 가설 자동 깔기)

고지형 클릭 → 흙토람 04/05/06/08을 고지형 판독 가설로 깐다. 06은 안쪽 구하도·가장자리 자연제방(좁으면 미저지).
사용자가 그린 면은 유지. 복원 아님. 제출 도메인 아님.
검증: cmake 0 · paleo seed 3 QTest PASS · smoke 0.
필드: 옛 창 닫고 아이콘 → 고지형. 커밋 금지.

## 지금 (2026-09-02 고지형 글자 과다)

스크린샷은 흙토람 토양도(산악지·곡간·선상 글자) + 빈 고지형 판독 범례. 복원 아님.
고침: 고지형 시 산악지 글자 제거, 4 ha 이상·입지후보만 라벨. 산지 색 흐리게. 빈 판독은 단일 심볼.
검증: cmake 0 · soil/paleo QTest 0 · smoke 0.
필드: 옛 창 닫고 아이콘 → 고지형·도면만들기. 커밋 금지.

## 지금 (2026-09-02 글자 토글 + 시굴 10%/표본 2%)

벡터 레이어 우클릭 「글자 끄기/켜기」. 토양·고지형 한글 라벨은 면은 두고 글자만 끔.
조사구역 레이어 우클릭 → 시굴격자 → 시굴(10%) / 표본(2%). 폭 2 m, 길이 배분. leftover union 없음.
지적 지번 글자·선은 VWorld 그림 → 글자만 분리 불가, 자석 불가.
고지형은 흙토람 04/05/06/08 강조(자동 적용). 옛 지형 복원 아님.
검증: cmake 0 · dem_trench_engine PASS · buffer_ring PASS · smoke · publish.
리뷰 후속: 라벨 없던 폴리곤에도 「글자 켜기」 (hasToggleableLabels). buffer_ring 0.
필드: 옛 창 닫고 아이콘. 커밋 금지. Autopilot PR 없음.

## 지금 (2026-09-01 자동저장 + 폴리곤 점 수정)

닫기·20초·저장 = persistSurveyWork (GPKG commit, 마지막 조사 재오픈). QGZ 대화상자 없음.
그리기 완료 후 꼭짓점 끌기 = KaCaptureMapTool hitSavedVertex + LayerOps::moveFeatureVertex.
검증: cmake 0 · recent 7 PASS · moveFeatureVertex PASS · smoke 0 · publish 0.
필드: 옛 창 닫고 아이콘. 커밋 금지.

## 지금 (2026-09-01 조판 +/테두리 자 끄기)

도면만들기 기본: 맵 안 LineCrosses(+)·바깥 좌표 주기(자) 꺼짐. PDF는 조판과 같은 격자 상태.
축척자·방위·CRS 글자는 유지. 격자 설정에서 다시 켤 수 있음.
검증: cmake 0 · drawingStudio_sheetOmitsCrossesAndBorderRuler PASS.
필드: 옛 창 닫고 아이콘 → 도면만들기 → PDF. 커밋 금지.

## 지금 (2026-09-01 시굴격자 = 현재 조사구역 / 지적 자석)

시굴격자 자동배치는 leftover `survey_area`를 union 하지 않는다. 선택한 구역, 없으면 마지막(fid 최대)만.
지적은 VWorld WMS/XYZ 그림 → 꼭짓점 자석 불가. 자석은 조사구역·유구·SHP/CAD Vertex+Segment.
검증: cmake 0 · dem_trench_engine 15 PASS · smoke 0 · publish 0.
필드: 옛 창 닫고 아이콘. 커밋 금지.



Cursor에서 Grok Build로 이 대화를 이을 때 **이 파일을 먼저** 읽는다.
커밋·리셋·force-push 금지. 기존 미커밋 파일은 절대 초기화하지 않는다.

## 지금 (2026-09-01 툴바 B Underline)

사용자 선택: B Underline. `:checked`는 파란 면 없이 아래 2px 선 + 남색 글자.
검증: cmake 0 · toolbarCheckedIsUnderline PASS · smoke 0 · publish 0.
필드: 옛 창 닫고 아이콘 — 파란 면 없이 밑줄만. 커밋 금지.

## 지금 (2026-09-01 툴바 Soft Chip)

## 지금 (2026-09-01 DEM 높이 구간 사용자 편집)

DEM 화살표 **높이 구간 바꾸기…**: 칸 수(2–12)·간격·여러 줄 하한/상한/색/이름을 한꺼번에 적용.
마지막 칸은 +inf / `N m 이상` (Discrete overflow 흰구멍 방지).
검증: cmake 0 · userClassCountAndCustomItems PASS · smoke 0 · publish 0.
필드: 옛 창 닫고 DEM 켠 뒤 화살표 → 높이 구간. 커밋 금지.

## 지금 (2026-09-01 DEM 흰구멍 + 바탕화면 포터블)

Discrete 마지막 칸을 `1e9` / `N m 이상` + `setClip(false)`. 샘플 통계보다 높은 봉우리도 색이 빠지지 않음. `/vsicurl` 전체 스캔 금지(sampleSize=200).
포터블: `publish-desktop` + 바탕화면 `ka-hgis-portable` (OneDrive 바탕 화면).
검증: cmake 0 · discreteFinerMeterClasses PASS · smoke 0 · publish 0 · robocopy 1.
필드: 옛 창 닫고 `ka-hgis-portable\start.bat` 또는 아이콘 **고고학 전용 HGIS**. DEM 끄고 다시 켜기.
커밋 금지.

## 지금 (2026-09-01 고지형 1단계 + DEM 레벨 세분)

- DEM 범례: Discrete 등간격. 화면/통계 범위가 좁으면 1–5 m 단, 타일 전체(~1000 m)면 50 m. 라벨 `12–22 m` (접미사 중복 없음).
- 툴바 **고지형**: 토양 04/05/06/08 강조 + `paleo_landform` 가설 판독(참조). 제출 도메인 아님.
- 항공사진 원클릭·palaeo-DEM·입체시 없음.
커밋 금지.

## 지금 (2026-09-01 DEM 범례·국토지리원 .img)

- 국토지리원 공개DEM(B080, ERDAS `.img`)은 **수동 다운로드만**. 클릭 자동 API 없음.
- DEM 본체 클릭: Copernicus GLO-30 (약 30 m, 높이 m 범례). 실패 시 GIBS 색타일.
- DEM 화살표: **국토지리원 DEM 불러오기(.img)** → 5 m급 도엽 + 범례 m.
검증: cmake 0 · `demElevationStyle_legendListsHeightMeters` PASS · smoke 0 · publish 0.
필드: 바탕화면 아이콘. 커밋 금지.

## 지금 (2026-09-01 지형맵 / DEM 분리)

사용자: OpenTopoMap은 **지형맵**. DEM은 색+음영 고도맵.
- 지형맵 = OpenTopoMap XYZ (등고·도로)
- DEM = NASA GIBS ASTER color shaded relief XYZ (`z/y/x`, zmax=12). VWorld WMS 금지.
검증: cmake 0 · `demColorRelief_is3857XyzNotTerrainMap` PASS · smoke-quit 0 · publish 0.
필드: 바탕화면 **고고학 전용 HGIS** → 지형맵 / DEM. 커밋 금지.

## 지금 (2026-09-01 고도맵 팬 크래시)

증상: 고도맵 켠 뒤 맵을 움직이면 **레이어가 꺼지는 게 아니라 프로그램 종료** (0xc0000005).
원인: VWorld WMS GetMap + OTF 5186 `QgsRasterProjector` + 팬 중 `provider_wms` `deleteLater` (crash-20260901-102801).
고침: 타일 오버레이는 **XYZ만**. `clampCanvasToKorea` / `extentsChanged`는 `isDrawing()`이면 `setExtent` 금지.
검증: cmake 0 · `elevationMap_xyzOnlySkipsAbortWhilePanning` PASS · smoke-quit 0 · publish 0.
필드: 바탕화면 **고고학 전용 HGIS**. 커밋 금지.

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
