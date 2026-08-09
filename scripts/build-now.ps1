$ErrorActionPreference = "Stop"
$env:PATH = "C:\ninja;C:\CMake\bin;" + $env:PATH
. "$PSScriptRoot\..\scripts\dev-env.ps1"
& C:\CMake\bin\cmake.exe -S "$PSScriptRoot\.." -B "$PSScriptRoot\..\build" -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DKA_HGIS_BUILD_TESTS=ON
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
& C:\CMake\bin\cmake.exe --build "$PSScriptRoot\..\build" --config Release -j 4
if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }
Write-Host "BUILD SUCCESS"
