$ErrorActionPreference = 'Stop'
$AppRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$errorLog = Join-Path $AppRoot 'startup_error.log'
$runtimeLog = Join-Path $AppRoot 'python_runtime_selection.log'

trap {
    ($_ | Out-String) | Set-Content -LiteralPath $errorLog -Encoding UTF8
    Write-Error $_
    exit 1
}

$settingsFiles = @(
    Get-ChildItem -LiteralPath $AppRoot -Directory |
        ForEach-Object { Get-ChildItem -LiteralPath $_.FullName -Directory -Filter 'qt_demo' } |
        ForEach-Object { Get-Item -LiteralPath (Join-Path $_.FullName 'settings.example.json') -ErrorAction SilentlyContinue }
)
if ($settingsFiles.Count -ne 1) {
    throw "Expected exactly one settings.example.json, found $($settingsFiles.Count)"
}
$settingsPath = $settingsFiles[0].FullName
$AppDir = $settingsFiles[0].DirectoryName
$appScript = Join-Path $AppDir 'app.py'
$sourceRuntime = Join-Path $AppRoot 'vendor\mvimpact_2_47_runtime'
$cacheRuntime = Join-Path $env:LOCALAPPDATA 'FX17UniformityRuntime\mvimpact_2_47_runtime'
$sourceCti = Join-Path $sourceRuntime 'bin\x64\mvGenTLProducer.cti'
$cacheCti = Join-Path $cacheRuntime 'bin\x64\mvGenTLProducer.cti'

foreach ($required in @($appScript, $settingsPath, $sourceCti)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Missing application input: $required"
    }
}

New-Item -ItemType Directory -Force -Path $cacheRuntime | Out-Null
& robocopy.exe $sourceRuntime $cacheRuntime /E /COPY:DAT /DCOPY:DAT /R:1 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
if ($LASTEXITCODE -ge 8) {
    throw "Camera runtime synchronization failed with robocopy exit code $LASTEXITCODE"
}
if (-not (Test-Path -LiteralPath $cacheCti -PathType Leaf)) {
    throw "Camera CTI was not created in the ASCII runtime cache: $cacheCti"
}

$utf8WithoutBom = New-Object System.Text.UTF8Encoding($false)
$settingsJson = [System.IO.File]::ReadAllText($settingsPath, $utf8WithoutBom)
$settings = $settingsJson | ConvertFrom-Json
$settings.cti_path = $cacheCti.Replace('\', '/')
$json = $settings | ConvertTo-Json -Depth 20
[System.IO.File]::WriteAllText($settingsPath, $json, $utf8WithoutBom)

$candidates = New-Object System.Collections.Generic.List[string]
if (-not [string]::IsNullOrWhiteSpace($env:FX17_PYTHON)) {
    $candidates.Add($env:FX17_PYTHON)
}
$candidates.Add('D:\Python39\python.exe')
$candidates.Add('C:\Python39\python.exe')
$candidates.Add((Join-Path $env:LOCALAPPDATA 'Programs\Python\Python39\python.exe'))

$selectedPython = $null
$checks = New-Object System.Collections.Generic.List[string]
$oldPythonPath = $env:PYTHONPATH
try {
    $env:PYTHONPATH = $AppDir
    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate) -or -not (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            $checks.Add("missing: $candidate")
            continue
        }
        $probe = & $candidate -c "import sys; assert sys.version_info[:2] == (3, 9); import PySide6, numpy, cv2, torch, openpyxl, harvesters, genicam; import simulation_app; print(sys.version.split()[0])" 2>&1
        if ($LASTEXITCODE -eq 0) {
            $selectedPython = (Resolve-Path -LiteralPath $candidate).Path
            $checks.Add("selected: $selectedPython ($probe)")
            break
        }
        $checks.Add("rejected: $candidate`n$($probe | Out-String)")
    }
} finally {
    $env:PYTHONPATH = $oldPythonPath
}

$checks | Set-Content -LiteralPath $runtimeLog -Encoding UTF8
if ($null -eq $selectedPython) {
    throw "No compatible Python 3.9 environment was found. See $runtimeLog. Set FX17_PYTHON to the validated python.exe path or restore D:\Python39\python.exe."
}

Remove-Item -LiteralPath $errorLog -Force -ErrorAction SilentlyContinue
$consoleLog = Join-Path $AppDir 'runtime_console.log'
$command = (
    '""' + $selectedPython + '" "' + $appScript +
    '" >> "' + $consoleLog + '" 2>&1"'
)
Start-Process -FilePath 'cmd.exe' -ArgumentList @(
    '/d',
    '/s',
    '/c',
    $command
) -WorkingDirectory $AppDir -WindowStyle Hidden
