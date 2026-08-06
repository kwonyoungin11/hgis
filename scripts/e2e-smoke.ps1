$ErrorActionPreference = "Continue"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root
. "$root\scripts\dev-env.ps1"
$ev = Join-Path $root ".omo\evidence\ka-hgis"
New-Item -ItemType Directory -Force -Path $ev | Out-Null
$log = Join-Path $ev "task-28-e2e.txt"
function Log([string]$m) { Add-Content -LiteralPath $log -Value $m; Write-Host $m }
Set-Content -LiteralPath $log -Value "=== ka-hgis e2e smoke ===" -Encoding utf8
Log "root=$root"

$rules = Join-Path $root "data\rules\drawing_checklist.v1.json"
if (-not (Test-Path $rules)) { Log "FAIL missing rules"; exit 1 }
Log "OK rules"
if (-not (Test-Path "$root\samples\demo_survey\demo.ka-survey.json")) { Log "FAIL demo"; exit 1 }
if (-not (Test-Path "$root\samples\bad_survey\bad.ka-survey.json")) { Log "FAIL bad"; exit 1 }
Log "OK samples"

$testExe = Join-Path $root "build\Release\ka_hgis_tests.exe"
if (-not (Test-Path $testExe)) { Log "FAIL unit tests missing"; exit 1 }
$p = Start-Process -FilePath $testExe -WorkingDirectory $root -Wait -PassThru -NoNewWindow -RedirectStandardOutput (Join-Path $ev "test-stdout.txt") -RedirectStandardError (Join-Path $ev "test-stderr.txt")
if ($p.ExitCode -ne 0) { Log "FAIL unit tests code=$($p.ExitCode)"; Get-Content (Join-Path $ev "test-stderr.txt") -ErrorAction SilentlyContinue | Select-Object -Last 20; exit $p.ExitCode }
Log "OK unit tests exit=0"

$app = Join-Path $root "build\Release\ka-hgis.exe"
if (-not (Test-Path $app)) { Log "FAIL app missing"; exit 1 }
$p2 = Start-Process -FilePath $app -ArgumentList "--smoke-quit" -WorkingDirectory (Split-Path $app) -Wait -PassThru -NoNewWindow -RedirectStandardOutput (Join-Path $ev "app-stdout.txt") -RedirectStandardError (Join-Path $ev "app-stderr.txt")
if ($p2.ExitCode -ne 0) { Log "FAIL smoke-quit code=$($p2.ExitCode)"; exit $p2.ExitCode }
Log "OK smoke-quit exit=0"

$j = Get-Content $rules -Raw | ConvertFrom-Json
if ($j.rules.Count -lt 12) { Log "FAIL rule count"; exit 1 }
Log "OK rule_count=$($j.rules.Count)"
Log "E2E PASS"
exit 0
