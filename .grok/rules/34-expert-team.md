# Expert team: ArcGIS + QGIS + developer

User standing order: **전문가 에이전트 팀** = ArcGIS 전문가 ∥ QGIS 전문가 ∥ 개발자.
Hooks inject this every prompt. Slash: `/ka-arcgis` `/ka-qgis` `/ka-developer`.

## Same-turn spawn (FEATURE / GIS / layout / digitize)

```
arcgis-expert  ∥  qgis-expert   (read-only)
ka-color ∥ ka-symbol ∥ ka-ui ∥ ka-ux
then TDD RED in tests/
then ka-developer  (or ka-implementer)
then ka-reviewer → ka-tester
```

`qgis-api` may substitute for `qgis-expert` on a pure wiring question.
`gis-protocol` / `field-check` still join when WMS/tiles/screenshots matter.

## Roles

| Agent | Job |
| --- | --- |
| `arcgis-expert` | Official Pro/developers practice + 용어.md. No ArcMap chrome. |
| `qgis-expert` | Official QGIS user manual + cookbook + api.qgis.org. No invented `Qgs*`. |
| `ka-developer` | TDD then smallest C++ in `src/core` preferred. |

Worker prompts: TASK / EXPECTED OUTCOME / MUST DO / MUST NOT DO / CONTEXT.
Do not narrate a launch without `spawn_subagent`.
