$ErrorActionPreference = "Continue"
$root = $env:GROK_WORKSPACE_ROOT
if (-not $root) { $root = $env:CLAUDE_PROJECT_DIR }
if (-not $root) { $root = (Get-Location).Path }

$state = Join-Path $root ".grok\.state"
New-Item -ItemType Directory -Force -Path $state | Out-Null
# New session = new turn. Keep last-verify (cross-turn evidence).
Set-Content -LiteralPath (Join-Path $state 'turn-src-writes.txt') -Value '' -Encoding ascii
Set-Content -LiteralPath (Join-Path $state 'turn-graph.txt') -Value '' -Encoding ascii

$clangd = Join-Path $env:USERPROFILE ".grok\tools\clangd\clangd.exe"
$cmakeCandidates = @(
    "C:\CMake\bin\cmake.exe",
    "$env:ProgramFiles\CMake\bin\cmake.exe",
    "${env:ProgramFiles(x86)}\CMake\bin\cmake.exe"
)
$cmake = $cmakeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
$lines = @()
if (Test-Path $clangd) { $lines += "clangd: ok" } else { $lines += "clangd: MISSING $clangd" }
if ($cmake) { $lines += "cmake: ok" } else { $lines += "cmake: MISSING (C:\\CMake\\bin or Program Files\\CMake)" }
$cc = Join-Path $root "build\compile_commands.json"
if (Test-Path $cc) { $lines += "compile_commands: ok" } else { $lines += "compile_commands: missing (run scripts/gen-compile-commands.ps1)" }

[Console]::Error.WriteLine(("ka-hgis session: " + ($lines -join "; ")))
$ctx = @'
[SESSION] Read .grok/NOW.md first (current work + do-not-touch). Excavation HGIS loop is ON via UserPromptSubmit hook.
Complexity 1-2: parent only. Complexity 3+: Task sequential-thinking-agent. context7 for library docs. ENGLISH official QGIS+ArcGIS developer sites first, expert team (arcgis-expert || qgis-expert then ka-developer) + designers, TDD tests-first, then implement.
좌표점=조판. 조판 축척 유지. 맵 중심→조판 중앙. /ka-predev /ka-tdd /ka-design
'@
$payload = @{
  hookSpecificOutput = @{
    hookEventName = 'SessionStart'
    additionalContext = $ctx
  }
} | ConvertTo-Json -Compress -Depth 5
[Console]::Out.Write($payload)
exit 0
