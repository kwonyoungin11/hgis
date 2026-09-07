# Pre-development setup (always, before any develop/fix)

User standing order: **개발사전설정을 규칙으로 항상 진행**. Slash: `/ka-predev`.

## Always

1. `context7` when the topic is a library API. STA (`sequential-thinking-agent`) if complexity ≥ 3. Skip both on greetings.
2. Official **English** QGIS + ArcGIS developer sites for the topic
   (`docs.qgis.org`, `api.qgis.org`, `docs/vendor/qgis-manual-3.44/`,
   `developers.arcgis.com`, `pro.arcgis.com`). Do not invent `Qgs*`.
3. Small plan (`.grok/rules/20-small-plan.md`): symptom, GIS object, files, done check.
4. Expert team: `arcgis-expert` ∥ `qgis-expert` ∥ then `ka-developer` (`.grok/rules/34-expert-team.md`).
5. Designers: `ka-color` ∥ `ka-symbol` ∥ `ka-ui` ∥ `ka-ux` (`.grok/rules/33-gis-design.md`).
6. TDD: failing test first (`.grok/rules/32-tdd.md`).
7. If compiling: `C:\CMake\bin` on PATH, `. .\scripts\dev-env.ps1`.

## Skip only

One-word greeting. Pure status with no edits.
