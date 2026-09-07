# Thin wrapper — SSOT is bin/prompt-inject.ps1
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$ssot = Join-Path (Split-Path -Parent $here) 'bin\prompt-inject.ps1'
if (Test-Path -LiteralPath $ssot) {
  & $ssot
  exit $LASTEXITCODE
}
# Fallback if bin is missing (user-level copy)
$ErrorActionPreference = 'SilentlyContinue'
[void][Console]::In.ReadToEnd()
$ctx = @'
[ORCHESTRATOR + GRAPH + LOOP — every user request]
MCP / skills / hooks are ON. Do not refuse them.
1) context7 when library docs are needed. STA (sequential-thinking-agent) only if complexity >= 3.
2) Internally rewrite the request. Do not dump it.
3) GRAPH: pick QUICK | gis | ship | debug | architecture and spawn that graph this turn. FEATURE must not be solo. Worker prompts: TASK / EXPECTED OUTCOME / MUST DO / MUST NOT DO / CONTEXT.
4) LOOP: after src/ edits, cmake --build then ctest this turn. Quote exit codes. No 완료/fixed without this-turn output. Fail → fix → re-run.
5) Invariants: no QGIS fork, no removeAllMapLayers, export EPSG:5179, no hardcoded VWorld key, no commit unless asked.
'@
$payload = @{
  hookSpecificOutput = @{
    hookEventName = 'UserPromptSubmit'
    additionalContext = $ctx
  }
} | ConvertTo-Json -Compress -Depth 5
[Console]::Out.Write($payload)
exit 0
