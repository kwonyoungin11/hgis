# Generate real CMake flags and preserve MSVC's implicit includes for editor clangd.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
. "$PSScriptRoot\dev-env.ps1"

$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vsWhere)) { throw "VS 2022 Build Tools / vswhere.exe not found." }
$vsInstall = & $vsWhere -latest -version '[17.0,18.0)' -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsInstall) { throw "VS 2022 C++ tools not found." }
$devShell = Join-Path $vsInstall 'Common7\Tools\Launch-VsDevShell.ps1'
& $devShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
if (-not $env:INCLUDE) { throw "MSVC/Windows SDK INCLUDE paths were not initialized." }
if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
  $ninjaDir = Join-Path $vsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'
  if (Test-Path -LiteralPath (Join-Path $ninjaDir 'ninja.exe')) { $env:PATH = "$ninjaDir;$env:PATH" }
}

Push-Location $Root
try {
  & cmake --preset compiledb
  if ($LASTEXITCODE -ne 0) { throw "CMake compile database configure failed." }
  $dbFile = Join-Path $Root 'build-clangd\compile_commands.json'
  $entries = @(Get-Content -LiteralPath $dbFile -Raw | ConvertFrom-Json)
  if ($entries.Count -eq 0) { throw "CMake generated an empty compiler database." }
  $includeArgs = @($env:INCLUDE -split ';' | Where-Object { $_ -and (Test-Path -LiteralPath $_) } |
    ForEach-Object { '/I"' + ($_ -replace '\\', '/') + '"' }) -join ' '
  foreach ($entry in $entries) {
    $entry.command += " $includeArgs"
  }
  $outDir = Join-Path $Root 'build'
  New-Item -ItemType Directory -Force -Path $outDir | Out-Null
  $outFile = Join-Path $outDir 'compile_commands.json'
  $json = ConvertTo-Json -InputObject $entries -Depth 6
  [System.IO.File]::WriteAllText($outFile, $json, [System.Text.UTF8Encoding]::new($false))
  Write-Host "CMake compiler database with MSVC/SDK includes: $outFile ($($entries.Count) entries)"
} finally {
  Pop-Location
}
