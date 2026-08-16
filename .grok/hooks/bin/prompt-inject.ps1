# UserPromptSubmit: orchestrator + graph engineering + loop engineering
$ErrorActionPreference = 'SilentlyContinue'
[void][Console]::In.ReadToEnd()
$ctx = @'
[ORCHESTRATOR + GRAPH + LOOP — every user request]
MCP / skills / hooks are ON. Do not refuse them.

1) Tools first: context7 and sequential-thinking (search_tool then use_tool) before answering.
2) Internally rewrite: Role, Context, Objective, Instructions, Constraints, Output, Tone, Evaluation, Edge Cases. Do not dump this rewrite.
3) GRAPH ENGINEERING — pick ONE graph this turn and run it:
   - QUICK (one file / typo / local string): parent only. No council.
   - gis (map / CRS / WMS / digitize / layout): spawn qgis-api ∥ gis-protocol ∥ field-check now (background: true).
   - ship / FEATURE (multi-file, studio, export, CRS convert): spawn ka-scout app ∥ ka-scout core same turn, then ka-implementer → ka-reviewer → ka-tester. Do not implement a FEATURE solo in the parent.
   - debug (crash / build / wrong GIS result): spawn ka-debugger ∥ ka-scout ∥ qgis-api if map.
   - architecture (IA / ADR / large UX): spawn ka-architect + plan; wait for the user before large edits.
   Emit spawn_subagent in this turn. Do not narrate a launch without the tool call.
   Worker prompts MUST include: TASK, EXPECTED OUTCOME, MUST DO, MUST NOT DO, CONTEXT (invariants).
   Activate matching skills: orchestrator-protocol, ka-graph, ka-experts, ka-hgis, ka-hgis-verify, gis-verify.
   Skip spawning only for a one-word greeting. Still do steps 1-2.
4) LOOP ENGINEERING — verify-fix-verify:
   After src/ C++ / QSS / UI edits: cmake --build build --config Release (touched target) then relevant ctest. Quote exit codes this turn.
   UI/boot/menu path: also scripts/run-ka-hgis.ps1 --smoke-quit when feasible.
   If verify fails: fix the first real error and re-run the same command. Do not finish on a red build.
   Do not claim 완료 / fixed / tests pass without this-turn command output.
   Docs/rules/hooks-only: say so; skip full app build.
5) Invariants: no QGIS fork, no removeAllMapLayers, no empty-layer auto-add, export EPSG:5179, no hardcoded VWorld key, no DXF submit, no commit unless asked.
'@

$payload = @{
  hookSpecificOutput = @{
    hookEventName = 'UserPromptSubmit'
    additionalContext = $ctx
  }
} | ConvertTo-Json -Compress -Depth 5

[Console]::Out.Write($payload)
exit 0
