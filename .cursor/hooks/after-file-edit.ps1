$ErrorActionPreference = 'Continue'
. (Join-Path $PSScriptRoot 'lib\state.ps1')

$evt = Get-KaCursorStdinJson
if (-not $evt) { exit 0 }

$path = [string]$evt.file_path
if (-not $path) { $path = [string]$evt.filePath }
if (-not $path) { $path = [string]$evt.path }

$added = Add-KaCursorSrcWrite $path

$edits = @()
if ($evt.edits) { $edits = @($evt.edits) }
foreach ($e in $edits) {
  $chunk = [string]$e.new_string
  if (-not $chunk) { $chunk = [string]$e.newString }
  if (Test-KaCursorSecretText $chunk) {
    Add-KaCursorSecretFlag $path
    break
  }
}

# afterFileEdit has no output fields; state files drive stop auto-review.
if ($added) { exit 0 }
exit 0
