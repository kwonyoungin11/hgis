$ErrorActionPreference = "Continue"
$root = $env:GROK_WORKSPACE_ROOT
if (-not $root) { $root = $env:CLAUDE_PROJECT_DIR }
if (-not $root) { $root = (Get-Location).Path }

$state = Join-Path $root ".grok\.state"
New-Item -ItemType Directory -Force -Path $state | Out-Null

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
[SESSION] Excavation HGIS loop is ON via UserPromptSubmit hook.
Every request: sequential-thinking + context7, ENGLISH official QGIS+ArcGIS developer sites first (docs.qgis.org, api.qgis.org, developers.arcgis.com, pro.arcgis.com), then implement. User command wins. skills, graph+loop, 3 next recs.
좌표점=조판. 조판 축척 유지. 맵 중심→조판 중앙.
'@
$payload = @{
  hookSpecificOutput = @{
    hookEventName = 'SessionStart'
    additionalContext = $ctx
  }
} | ConvertTo-Json -Compress -Depth 5
[Console]::Out.Write($payload)
exit 0
