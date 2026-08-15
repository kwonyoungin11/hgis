# 한 번 난 오류 — 재발 금지

같은 증상이 다시 나오면 이 표의 **고정 방법**을 먼저 확인한다.
새 오류를 고치면 이 파일과 `tests/test_workflow.cpp`에 회귀 테스트를 추가한다.

| 증상 | 원인 | 재발 방지 |
| --- | --- | --- |
| 위성/지적 버튼을 눌러도 지도가 안 보임 | 공식 WMTS URI가 `isValid()`만 통과하고 타일은 실패. 폴백 xdworld를 안 탐. 지적 WMS를 5186/5179로 먼저 열어 `Cannot calculate extent`. | 위성은 xdworld를 먼저. 지적은 `cadastralWmsCrsCandidates()`가 **4326→3857→900913만**. 5186/5187/5179 금지. 테스트: `cadastralWmsCrs_neverStartsWithWorkCrs5179`, `addVworldSatellite_allowsEmptyKeyViaPublicTiles` |
| 지적 레이어는 범례에 있는데 필지가 안 보임 | 구버전(eac6c9c)은 `crs=EPSG:3857` + `tilePixelRatio=2` + `KEY/DOMAIN` tiled WMS. 이후 GDAL/4326/5179 경로가 그걸 깨뜨림. | 구버전 URI로 복구. GDAL은 최후 폴백만. 테스트: `cadastralWmsCrs_neverStartsWithWorkCrs5179` |
| 「이 레이어만 보기」가 그 레이어로 안 감 | `QgsRectangle::isEmpty()`가 점/축평행 선을 “도형 없음”으로 처리. 우클릭 선택을 `QgsLayerTreeModel`로 캐스팅해 실패. | `isNull()`/`isFinite()`만 실패. `index2node` + `currentLayer()`. 줌 실패 시 isolate 안 함. 테스트: `zoomToLayerMax_movesCanvasToPointFeature`, `isolateAndZoom_hidesOtherSurveyKeepsReference` |
| “실행했다”는데 창이 없음 | 에이전트 Job Object가 자식 프로세스를 같이 종료. `exe` 직접 실행은 OSGeo DLL 경로 없음. | `고고학 전용 HGIS.lnk` / `scripts/start-ka-hgis.vbs` / `launch.ps1`. explorer로 분리 실행. |
| 조판 축척자가 0 m, 지적 안 보임, 범례 크기 무시 | 지도 칸이 **선택된** 레이어만 씀. `setLayers` 순서가 해시라 위성이 지적 위. 축척자 `applyDefaultSize` 없음. 범례 `resizeToContents`가 드래그 크기를 덮음. | `visibleLayersPaintOrder`(조사→지적→위성). 체크된 레이어만. 축척자 `setLinkedMap`+`applyDefaultSize`. 범례 `setResizeToContents(false)`. 설정은 즉시 반영. 테스트: `layoutStudio_checkedLayersScaleBarLegendSize` |
| 조판에서 용지 안 범위를 그리면 다운 | (1) QGIS 고무줄 `layout()` null 역참조. (2) `setLayers({})` = 위성·지적 WMS 전체. (3) `setExtent`가 칸 높이를 다시 씀. (4) `QgsLayoutViewToolSelect::setLayout` 미호출 → `mMouseHandles` null. (5) `setCurrentLayout(nullptr)`는 `pageCollection()`을 무조건 탐. | QGIS 고무줄 금지. `zoomToExtent`. 빈 레이어 금지. WMS는 큐 이후. `m_toolSelect->setLayout(ly)`. 용지 교체는 `setScene(nullptr)` 후 새 시트. `refresh()` 금지. 테스트: `layoutBlankSheetMapItemKeepsFrameAndNonEmptyLayers` |
| 다른 좌표계가 안 겹침 | QGIS 3는 항상 OTF. `ensureOtfEnabled`가 작업 CRS(5186/5187)를 캔버스 목적 CRS로 두고, 위성 3857은 재투영한다. | `ensureOtf_projectAndCanvasDestinationCrs5186`, `syncMapCanvas_surveyLayersAboveReferenceBasemap`. 업로드만 5179. |
| 배경/위성이 안 보임 (키는 있는데) | 위성이 xdworld를 먼저 열어 `isValid()`만 통과하고, 저장된 VWorld 키 WMTS를 안 탐. | 키가 있으면 `api.vworld.kr` WMTS를 먼저. xdworld는 키 없을 때만. 테스트: `addVworldSatellite_usesOfficialWmtsWhenKeyPresent` |

## 지적 GetMap (확인된 사실)

- 키 + `domain=localhost`로 GetCapabilities에 `lp_pa_cbnd_bonbun` 있음. bbox는 **EPSG:4326만**.
- 약 600m 창 EPSG:3857 GetMap은 주황 필지선 PNG.
- 수십 km 창은 투명 빈 PNG. 그래서 한국 전체 축척에서는 지적 선이 안 나온다.
