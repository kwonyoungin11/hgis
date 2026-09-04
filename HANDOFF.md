# ka-hgis Handoff — product SSOT (v0.3.0)

> Updated: 2026-08-22. Grok Build (Cursor 포함). 현재 세션: [`.grok/NOW.md`](./.grok/NOW.md)

**Agent rules:** [`AGENTS.md`](./AGENTS.md)  
**This file + `docs/HANDOFF.md`:** edit together.

---

## Resume after reconnect (same PC or clone)

이 PC 작업본: `A:\hgis - 복사본` · OSGeo **`A:\OSGeo4W`**. 세션 재개 시 `.grok/NOW.md`부터.

Remote: `https://github.com/kwonyoungin11/hgis.git`

```powershell
cd "A:\hgis - 복사본"
$env:PATH = "C:\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1
.\scripts\run-ka-hgis.ps1
```

Launch for the field user (do **not** start `ka-hgis.exe` raw):

- Desktop **고고학 전용 HGIS.lnk** → `dist\ka-hgis-portable\start.bat`
- After C++/UI builds: `.\scripts\publish-desktop.ps1` then click that icon
- or `.\scripts\start-ka-hgis.vbs` → `launch.ps1` (uses `build\Release` first)

Other PC first time: `docs/other-pc-setup.md` · `.\scripts\bootstrap-dev-pc.ps1`  
VWorld key is **local only** (`VworldSettings` / 도움말 → API 키). Never commit `config/secrets.ini`.

---

## 1. Product

Korean field archaeology HGIS (C++20/Qt6 + OSGeo4W `qgis-dev`, Architecture B, no QGIS fork):

1. GPKG survey store; **legend empty until draw/import** (`LayerOps::ensureDomainLayer` only). **새 조사** drops every non-basemap layer (keep WMS/XYZ 지적·위성 only)
2. Domain keys: `survey_area`, `feature_poly`, `feature_line`, `section_line`, `control_points`, `artifact_point`
3. **참조 지도** (위성/지적) vs **조사 데이터**
4. Digitize: startEditing → addFeature → commit (keep tool). After a polygon is finished, drag a saved vertex to reshape. **Ctrl+Z** undoes last vertex, then last saved feature. Close/20s timer/`저장` = `persistSurveyWork` (GPKG commit, last survey reopen). No QGZ dialog on 저장.
5. **도면 만들기** = `KaDrawingStudio` (not QGIS Layout Designer). Samples for north/scale/legend/CRS. **Ctrl+Z** removes last placed item. 좌표점은 지도 칸만, 우클릭/Delete/Ctrl+Z로 마지막 점 삭제. 축척·이동 후 땅 XY에서 다시 앉힘. 자석은 조사 벡터 꼭짓점·선(지적 WMS 그림은 불가). 전문 도곽(+ 십자·테두리 좌표 자)은 **격자 설정에서 켤 때만**. 도면만들기 기본·PDF는 맵 테두리만(축척자·방위·CRS는 유지). 자동 도면(`fillLayout`)은 표제란(도면명·조사명·축척·좌표계·작성일) 포함. 도면의 래스터(위성·지적·지질)는 조각 렌더 없이 한 번에 그린다(`LayoutService::applySingleRasterPassRendering`) — QGIS 기본 조각 렌더는 조각 하나가 비면 위성이 반만 나온 것처럼 보인다
5c. **단면도** = `KaSectionDrawingStudio` 전용 탭. 열면 A3/A4 용지와 표고·거리 눈금이 이미 있다. 좌측 **GeoTIFF 추가**만 쓰고 위성·지적·조사 벡터는 목록에 넣지 않는다. CRS는 EPSG:5187/5186 선택(재투영 없음, 거리×표고 m). PDF는 `SectionLayoutService::exportSectionPdf`(300 DPI, forceVector, AlwaysText)
5b. **맞추기** = same-canvas JPG/DXF onto 지적 (Fit To Display + 2–3 pairs). Result is **참조 지도**, not submit geometry. Not GNSS `control_points`. **이동**은 같은 `QgsRasterLayer`에 월드파일만 적용(`persistAlignedRaster`). `removeMapLayer`+재오픈·`refreshAllLayers`/`zoomToLayerMax` 금지(지적 WMS 그리는 중 멈춤). 화살표/번호는 화면 캐시가 아니라 매번 `mapX/mapY`에서 다시 찍는다. 흰 종이 스캔만 먹선 Multiply. **항공 GeoTIFF는 원색 SourceOver**(대비·감마·흰 픽셀 투명을 주지 않음).
5d. **고지형** = `PaleoLandformService`. 흙토람 04/05/06/08 강조 + `seedInterpretationFromSoil` 가설 자동 깔기(04 선상지·05 해성평탄·08 하안단구·06 구하도/자연제방 또는 좁으면 미저지). `note` `자동:`만 재실행 시 교체. 토양 글자는 큰 입지후보만. **참조 지도**, not domain, not 5179. Not palaeo-DEM / 입체시 / 복원 주장 금지.
5g. **토양도** = WFS `SOIL_1/2/3`(분포지형 속성·고지형) 아래 흙토람과 같은 `TOP_A_SOIL_T_GEO` 3857 그림(산능선 산악지). 목록에 벡터+그림 두 줄. WMS `crs=`에 5186 금지. 제출 도메인 아님. 조판 범례는 레이어 체크와 같다(`tuneSheetLegend`). 끈 레이어·그림·위성·지적 이름은 범례에 넣지 않음.
5h. **지질도 입체** = `GeologyMapService::ensureReliefUnderlay`. 지질 색(KIGAM 1:5만) **위**에 `지질 음영`(로컬 DEM hillshade 또는 Esri World_Hillshade XYZ). 음영 Multiply·opacity 0.48, 지질 색 SourceOver. 산이 솟는 3D 렌더 아님. **참조 지도**. 조판 범례에서 음영 이름 생략. 다시 누르면 지질+음영 같이 숨김.
5i. **입체지형** = 리본 진입 **삭제**(사용자 포기). 엔진/`terrain3d_sheet` 코드는 남김. 제출 5179 아님.
5e. **레이어 글자** = 벡터 우클릭 「글자 끄기/켜기」(`LayerOps::setLabelsVisible`). 지적 WMS의 지번 글자는 그림에 박혀 있어 레이어를 통째로 꺼야 함.
5f. **시굴격자 비율** = 조사구역 레이어 우클릭 → 시굴격자 → 시굴(10%) / 표본(2%). 폭 2 m 고정, 길이·간격 배분(`buildForTargetRatio`). 선택한(없으면 마지막) 구역만.
6. Work CRS default **EPSG:5187 (동부)**; 5186 also OK. **export SHP+PDF+MANIFEST = EPSG:5179**. Checklist error hard-blocks 제출
7. 초보자 리본 6그룹(조사파일/기록/배경/정합·분석/산출/찾기). 질문형 짧은 라벨. Text menu bar **hidden**. **파일함은 기본 표시**(더보기로 숨길 수 있음). 작업 제어 dock은 더보기. 조판 위 「보기」 리본 없음 — 줌·이동은 마우스, 용지 맞춤은 열 때 자동.

---

## 2. What landed (do not rebuild)

| Area | Notes |
| --- | --- |
| VWorld 위성 | Stored API key → `api.vworld.kr` WMTS **first**; xdworld only if no key |
| VWorld 지적 | Frozen tiled WMS `crs=EPSG:3857` + KEY/DOMAIN. Do not put 5186/5187/5179 in WMS CRS list |
| Digitize / attrs | `KaCaptureMapTool`, `KaAttributeMapTool`, `ensureDomainLayer`. 그리기: 조사구역/유구면/유구선/단면선/기준점. 레이어 삭제·그린 도형 삭제는 `purgeCommittedFeatures`(GPKG 비움). 도형수정 자석=`snapToMap`, 선 우클릭 점추가/점삭제. 시굴격자 자동배치는 **선택한(없으면 마지막) survey_area만** — leftover union 금지. 자석은 벡터 꼭짓점·선만(지적 WMS 그림에는 안 붙음) |
| Layout studio | `src/app/KaDrawingStudio.*` — 160 mm scale bar, PNG north = sample, CRS label |
| Section studio | `src/app/KaSectionDrawingStudio.*` + `src/core/SectionLayoutService.*` — A3/A4 landscape, GeoTIFF body, elevation/distance ticks, vector PDF |
| Terrain 3D tab | `src/app/KaTerrain3dStudio.*` + `src/core/Terrain3dService.*` — 전용 **입체지형** 탭. **지금 지도 화면**을 고해상 메시+Google 위성으로. DEM 파일 열기는 보조. `qgis_3d` 없음 |
| Launch | `scripts/start-ka-hgis.vbs` + `launch.ps1` (Job Object safe) |
| Chrome theme | `KaTheme` + `data/theme/ka-hgis.qss` sky 3D / black 2px regions |
| Tests | `tests/test_workflow.cpp` (satellite key-first, undo feature, scale bar width, …) |

---

## 3. Next (if continuing production)

1. Export package PDF should be the **composed studio sheet**, not `rebuildDefaultLayouts` 5 templates
2. Checklist `layout_exists:*` must not pass on empty auto-seeded layouts
3. Do **not**: QGIS fork, DXF submit, restore 7-step rail, change cadastral WMS recipe, hardcode VWorld key

---

## 4. Layout / build

```
src/app/MainWindow.*        chrome, digitize, export
src/app/KaDrawingStudio.*   조판
src/app/KaSectionDrawingStudio.* 단면도 탭
src/core/SectionLayoutService.* 단면 눈금·조판·PDF
src/app/KaCaptureMapTool.*  그리기
src/app/KaAttributeMapTool.* 속성 클릭
src/core/LayerOps.*         basemap, layer_key, undoCommittedFeature
src/core/PaleoLandformService.* 고지형 1단계 (참조 판독)
src/core/ExportService.*    SHP 5179 + MANIFEST
src/core/LayoutService.*    default 5 layouts (package PDF still uses these)
src/core/ChecklistEngine.*
tests/test_workflow.cpp
```

```
$env:PATH = "C:\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1
cmake --build build --config Release
.\scripts\run-ka-hgis.ps1
```

This machine: OSGeo `A:\OSGeo4W` (dev-env also accepts C: / D:). CMake `C:\CMake\bin`.

---

## 5. Grok Build

| Surface | Path |
| --- | --- |
| Agent rules | `AGENTS.md` |
| Session now | `.grok/NOW.md` |
| Cursor → Grok | `.cursor/rules/grok-ka-hgis.mdc` |
| Section invariants | `.grok/rules/40-section-studio.md` |
| Drawing-studio invariants | `.grok/rules/41-drawing-studio.md` |
| Product SSOT | `HANDOFF.md` + `docs/HANDOFF.md` |
| Preset (MCP/skills/hooks/LSP/graph) | `.grok/rules/00-grok-preset.md` |
| Graph + loop (fail closed) | `.grok/rules/50-graph-loop.md` · stop-gate this-turn writes |
| clangd | `.clangd` + `.grok/lsp.json` |
| Skills | `/ka-experts` `/ka-graph` `/gis-verify` `/ka-hgis-verify` `/ka-drawing-studio` `/ka-submit-package` `/ka-georef-align` |
| Experts | `ka-scout` `ka-implementer` `ka-reviewer` `ka-debugger` `ka-architect` `ka-tester` `qgis-api` `gis-protocol` `field-check` |
| Workflows | `/workflow ka-ship` `/workflow ka-council` `/workflow ka-verify` |
| Orca worktree setup | `orca.yaml` + `scripts/orca-worktree-setup.ps1` |
