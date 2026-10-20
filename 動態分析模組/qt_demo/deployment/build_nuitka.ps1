param(
    [switch]$SkipTests,
    [switch]$Console
)

$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$qtDir = Split-Path -Parent $scriptDir
$moduleDir = Split-Path -Parent $qtDir
$bundleRoot = Split-Path -Parent $moduleDir
$venvPython = Join-Path $bundleRoot '.venv\Scripts\python.exe'
if (-not (Test-Path -LiteralPath $venvPython)) {
    throw "Missing bundle virtual environment: $venvPython"
}

if (-not $SkipTests) {
    & (Join-Path $scriptDir 'run_all_tests.ps1') -LoadModels
}

& $venvPython -m pip install -r (Join-Path $qtDir 'requirements-build.txt')

$buildDir = Join-Path $bundleRoot 'build\nuitka'
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
$previousPythonPath = $env:PYTHONPATH
try {
    $modelCodeDirectories = Get-ChildItem -LiteralPath $bundleRoot -Directory |
        Where-Object {
            (Test-Path -LiteralPath (Join-Path $_.FullName 'auto_sam_mask.py')) -or
            (Test-Path -LiteralPath (Join-Path $_.FullName 'predict_resistance.py'))
        }
    $pythonPaths = @($qtDir) + @($modelCodeDirectories.FullName)
    $env:PYTHONPATH = $pythonPaths -join ';'
    $nuitkaArgs = @(
        '--standalone',
        '--enable-plugin=pyside6',
        '--msvc=latest',
        '--include-package=torch',
        '--include-package=harvesters',
        '--include-package=genicam',
        '--include-package=cv2',
        '--include-package=openpyxl',
        '--include-module=auto_sam_mask',
        '--include-module=infer_cae',
        '--include-module=train_cae',
        '--include-module=predict_resistance',
        '--include-module=train_resistance_model',
        '--include-module=thickness_classifier',
        '--output-filename=FX17Analysis.exe',
        "--output-dir=$buildDir"
    )
    if (-not $Console) {
        $nuitkaArgs += '--windows-console-mode=disable'
    }
    $nuitkaArgs += (Join-Path $qtDir 'app.py')
    & $venvPython -m nuitka @nuitkaArgs
    if ($LASTEXITCODE -ne 0) { throw 'Nuitka build failed.' }
} finally {
    $env:PYTHONPATH = $previousPythonPath
}

$distDir = Get-ChildItem -LiteralPath $buildDir -Directory -Filter '*.dist' |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if ($null -eq $distDir) { throw 'Nuitka output *.dist was not found.' }

$releaseRoot = Join-Path $bundleRoot 'release\FX17_camera_app'
$resolvedBundle = [System.IO.Path]::GetFullPath($bundleRoot)
$resolvedRelease = [System.IO.Path]::GetFullPath($releaseRoot)
if (-not $resolvedRelease.StartsWith($resolvedBundle, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Release path escaped the bundle root.'
}
if (Test-Path -LiteralPath $releaseRoot) {
    Remove-Item -LiteralPath $releaseRoot -Recurse -Force
}

$releaseModule = Join-Path $releaseRoot (Split-Path -Leaf $moduleDir)
$releaseQt = Join-Path $releaseModule 'qt_demo'
New-Item -ItemType Directory -Force -Path $releaseQt | Out-Null
Copy-Item -Path (Join-Path $distDir.FullName '*') -Destination $releaseQt -Recurse -Force
Copy-Item -LiteralPath (Join-Path $qtDir 'settings.example.json') -Destination $releaseQt -Force
Copy-Item -LiteralPath (Join-Path $bundleRoot 'FX17_HANDOFF.md') -Destination $releaseRoot -Force
$assetDirectories = Get-ChildItem -LiteralPath $bundleRoot -Directory |
    Where-Object {
        (Test-Path -LiteralPath (Join-Path $_.FullName 'auto_sam_mask.py')) -or
        (Test-Path -LiteralPath (Join-Path $_.FullName 'predict_resistance.py')) -or
        (Test-Path -LiteralPath (Join-Path $_.FullName '50u_1.hdr'))
    }
foreach ($sourceDirectory in $assetDirectories) {
    Copy-Item -LiteralPath $sourceDirectory.FullName -Destination $releaseRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $releaseModule | Out-Null
foreach ($relative in @('fx_test', 'calibration')) {
    Copy-Item -LiteralPath (Join-Path $moduleDir $relative) -Destination $releaseModule -Recurse -Force
}

Write-Output "Nuitka standalone release: $releaseRoot"
Write-Output 'Do not distribute it until the source app has passed the real conveyor test.'
