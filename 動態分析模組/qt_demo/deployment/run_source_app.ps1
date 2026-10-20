$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$qtDir = Split-Path -Parent $scriptDir
$bundleRoot = Split-Path -Parent (Split-Path -Parent $qtDir)
$pythonExe = Join-Path $bundleRoot '.venv\Scripts\python.exe'
if (-not (Test-Path -LiteralPath $pythonExe)) {
    throw 'Please run install_environment.ps1 first.'
}
Push-Location $qtDir
try {
    & $pythonExe app.py
} finally {
    Pop-Location
}
