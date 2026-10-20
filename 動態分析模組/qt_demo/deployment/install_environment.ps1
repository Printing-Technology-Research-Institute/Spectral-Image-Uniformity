param(
    [switch]$CpuOnly
)

$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$qtDir = Split-Path -Parent $scriptDir
$bundleRoot = Split-Path -Parent (Split-Path -Parent $qtDir)
$venvDir = Join-Path $bundleRoot '.venv'
$pythonExe = Join-Path $venvDir 'Scripts\python.exe'

if (-not (Test-Path -LiteralPath $pythonExe)) {
    py -3.9 -m venv $venvDir
}

& $pythonExe -m pip install --upgrade pip setuptools wheel
if ($CpuOnly) {
    & $pythonExe -m pip install -r (Join-Path $qtDir 'requirements-camera.txt')
} else {
    & $pythonExe -m pip install -r (Join-Path $qtDir 'requirements-cuda126.txt')
}

& $pythonExe -B (Join-Path $scriptDir 'check_site_environment.py')
