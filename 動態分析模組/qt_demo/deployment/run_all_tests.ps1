param(
    [switch]$LoadModels
)

$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$qtDir = Split-Path -Parent $scriptDir
$bundleRoot = Split-Path -Parent (Split-Path -Parent $qtDir)
$pythonExe = Join-Path $bundleRoot '.venv\Scripts\python.exe'
if (-not (Test-Path -LiteralPath $pythonExe)) {
    $pythonExe = (Get-Command python).Source
}

$previousQtPlatform = $env:QT_QPA_PLATFORM
try {
    $env:QT_QPA_PLATFORM = 'offscreen'
    Push-Location $qtDir
    & $pythonExe -B -m unittest discover -p 'test_*.py' -v
    if ($LASTEXITCODE -ne 0) { throw 'Unit tests failed.' }
    Pop-Location

    $checkArgs = @((Join-Path $scriptDir 'check_site_environment.py'))
    if ($LoadModels) { $checkArgs += '--load-models' }
    & $pythonExe -B @checkArgs
    if ($LASTEXITCODE -ne 0) { throw 'Environment check failed.' }
} finally {
    if ((Get-Location).Path -eq $qtDir) { Pop-Location }
    $env:QT_QPA_PLATFORM = $previousQtPlatform
}
