# Synthesize build/compile_commands.json for clangd when the VS generator
# does not emit one. Prefer an existing Ninja compile DB if present.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$OutDir = Join-Path $Root "build"
$OutFile = Join-Path $OutDir "compile_commands.json"
$FlagsFile = Join-Path $Root "compile_flags.txt"

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$ninjaDb = @(
    (Join-Path $Root "build-clangd\compile_commands.json"),
    (Join-Path $OutDir "compile_commands.json")
)
foreach ($cand in $ninjaDb) {
    if ((Test-Path $cand) -and ((Get-Item $cand).Length -gt 80)) {
        if ($cand -ne $OutFile) {
            Copy-Item $cand $OutFile -Force
        }
        Write-Host "compile_commands.json ready: $OutFile"
        exit 0
    }
}

if (-not (Test-Path $FlagsFile)) { throw "missing $FlagsFile" }

$flags = Get-Content $FlagsFile | Where-Object { $_.Trim() -ne "" }
$srcRoot = $Root -replace '\\', '/'
$args = New-Object System.Collections.Generic.List[string]
$args.Add("clang++")
foreach ($f in $flags) { $args.Add($f) }

$files = Get-ChildItem -Path (Join-Path $Root "src") -Recurse -Include *.cpp, *.h, *.hpp
$entries = @()
foreach ($f in $files) {
    $path = ($f.FullName -replace '\\', '/')
    $entries += [ordered]@{
        directory = $srcRoot
        file      = $path
        arguments = @($args + @("-c", $path))
    }
}

$json = $entries | ConvertTo-Json -Depth 6
# ConvertTo-Json on a single-element array may unwrap; force array wrapper
if ($entries.Count -eq 1) { $json = "[$json]" }
[System.IO.File]::WriteAllText($OutFile, $json)
Write-Host "wrote $($entries.Count) entries -> $OutFile"
exit 0
