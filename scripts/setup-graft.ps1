# Project-local, pinned Graft install. Never runs graft init or model-backed builds.
param([switch]$Rebuild)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$toolRoot = Join-Path $repoRoot 'build\tooling\graft-source'
$graftCommit = 'f9e65396e638e517aecae0d731017f53084d70ed'
$graftRemote = 'https://github.com/NanoNets/Graft.git'
foreach ($command in @('git', 'node', 'npm.cmd')) {
  if (-not (Get-Command $command -ErrorAction SilentlyContinue)) { throw "Required tool missing: $command" }
}
$previousCI = $env:CI
$previousTracking = $env:DO_NOT_TRACK
$env:CI = '1'
$env:DO_NOT_TRACK = '1'
try {
  if (-not (Test-Path -LiteralPath $toolRoot)) {
    New-Item -ItemType Directory -Force (Split-Path -Parent $toolRoot) | Out-Null
    & git clone --no-checkout $graftRemote $toolRoot
    if ($LASTEXITCODE -ne 0) { throw 'Graft clone failed.' }
    & git -C $toolRoot checkout --detach $graftCommit
    if ($LASTEXITCODE -ne 0) { throw 'Pinned Graft checkout failed.' }
  }
  $actualCommit = & git -C $toolRoot rev-parse HEAD
  if ($LASTEXITCODE -ne 0 -or $actualCommit -ne $graftCommit) { throw "Unexpected Graft checkout: $actualCommit" }
  $dirty = & git -C $toolRoot status --porcelain --untracked-files=no
  if ($LASTEXITCODE -ne 0 -or $dirty) { throw 'Graft source has changes; inspect them before rebuilding.' }
  Push-Location $toolRoot
  try {
    if ($Rebuild -or -not (Test-Path -LiteralPath 'dist\mcp\tools.js') -or
        -not (Test-Path -LiteralPath 'dist\graph\build.js') -or
        -not (Test-Path -LiteralPath 'node_modules\tree-sitter\package.json')) {
      & npm.cmd ci --ignore-scripts --no-audit --no-fund
      if ($LASTEXITCODE -ne 0) { throw 'Graft dependencies failed.' }
      & npm.cmd rebuild tree-sitter-kotlin tree-sitter-swift tree-sitter-r --foreground-scripts
      if ($LASTEXITCODE -ne 0) { throw 'Graft native parser build failed. Check Python and VS C++ tools.' }
      & npm.cmd run build
      if ($LASTEXITCODE -ne 0) { throw 'Graft TypeScript build failed.' }
    }
  } finally { Pop-Location }
  & node (Join-Path $PSScriptRoot 'graft-mcp.mjs') --index
  if ($LASTEXITCODE -ne 0) { throw 'HGIS structural index failed.' }
  Write-Host "Graft $graftCommit ready: $toolRoot"
} finally {
  $env:CI = $previousCI
  $env:DO_NOT_TRACK = $previousTracking
}
