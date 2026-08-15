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
exit 0
