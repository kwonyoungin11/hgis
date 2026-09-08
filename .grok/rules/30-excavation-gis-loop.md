# Excavation GIS loop (user-forced, every request)

This is the user's standing order for **문화재 발굴조사 HGIS**. Hooks inject the same text on `UserPromptSubmit` (`.grok/hooks/bin/prompt-inject.ps1`).

## Every request

1. Prompt-engineer internally (do not dump).
2. context7 when library docs are needed. STA (`sequential-thinking-agent`) if complexity ≥ 3. Do not call sequential-thinking MCP on every request.
3. Follow the user's last command. Before map/CRS/layout/digitize/graphic edits, learn the topic in **English** from official developer sites (context7 + fetch): QGIS cookbook/user manual/api.qgis.org + in-repo `docs/vendor/qgis-manual-3.44/`; ArcGIS `developers.arcgis.com` + Pro mapping help. Terms: `docs/user/job-cards/arcgis-용어.md`. Do not invent Qgs* APIs. Do not clone ArcMap chrome.
4. Load matching skills. Each expert loads skills **separately**.
5. Graph: QUICK solo; FEATURE/gis/debug/architecture summon the full matching expert set this turn (`spawn_subagent` or `/workflow ka-ship`). Editing app+core without a spawn is blocked.
   Always also: expert team (`arcgis-expert` ∥ `qgis-expert` then `ka-developer`) + designers; TDD tests-first; predev `.grok/rules/21-predev.md`.
6. Loop: this-turn `src/` writes need build + tests this turn. Evidence before 완료. Docs/hooks-only skip cmake. Leftover dirty `src/` does not block research. See `.grok/rules/50-graph-loop.md`.
7. Cross-check against existing digitize / legend / layout / export 5179. Ship only if usable.
8. Persist decisions (memory / this rule). Remember prior asks.
9. End with **3** next excavation-GIS recommendations.

## Product invariants

Architecture B (link qgis_core/qgis_gui). No fork. No `removeAllMapLayers` on survey load. Legend empty until draw/import. Work 5186/5187, upload **EPSG:5179**. No hardcoded VWorld key. No DXF submit. No commit unless asked.

## Remembered user product asks

- 좌표점: **조판**에서 찍는다 (맵 아이콘 없음).
- 맵↔조판: 조판 축척 유지. 조판 진입 시 맵 **중심**만 조판 중앙으로. 레이어를 가운데 = 축척 유지 + 중심 이동.
- 시작: 위성·지적 자동, 홈/최근 조사, 스플래시.
- 툴바: 도형선택, 폴리곤그리기, 맞추기, 주변유적 경계표시, 도면만들기. 시작 축척 1:25000.
- 조판 좌표점/가운데/선택은 용지 아래 가운데 아이콘. PDF는 범례 옆.
