param(
    [string]$PythonPath = "C:\Users\HSIPL\AppData\Local\Programs\Python\Python39\python.exe"
)

$ErrorActionPreference = "Stop"
$serviceName = "mxssvr"
$diagnosticPath = Join-Path $PSScriptRoot "fx17_diagnostic.py"
$singleReportPath = Join-Path $PSScriptRoot "fx17_diagnostic_report.json"
$stabilityReportPath = Join-Path $PSScriptRoot "fx17_diagnostic_1000_report.json"
$logPath = Join-Path $PSScriptRoot "test_fx17_without_ni_service.log"
$exitCode = 1
$serviceWasRunning = $false

Start-Transcript -Path $logPath -Force | Out-Null
try {
    if (-not (Test-Path -LiteralPath $PythonPath -PathType Leaf)) {
        throw "Python executable not found: $PythonPath"
    }
    if (-not (Test-Path -LiteralPath $diagnosticPath -PathType Leaf)) {
        throw "FX17 diagnostic not found: $diagnosticPath"
    }

    $service = Get-Service -Name $serviceName
    $serviceWasRunning = $service.Status -eq "Running"
    Write-Output "Initial $serviceName status: $($service.Status)"

    if ($serviceWasRunning) {
        Stop-Service -Name $serviceName -Force
        (Get-Service -Name $serviceName).WaitForStatus(
            [System.ServiceProcess.ServiceControllerStatus]::Stopped,
            [TimeSpan]::FromSeconds(20)
        )
    }
    Write-Output "Test-time $serviceName status: $((Get-Service -Name $serviceName).Status)"

    & $PythonPath $diagnosticPath `
        --buffers 1 `
        --timeout 1.0 `
        --report $singleReportPath
    $singleExitCode = $LASTEXITCODE
    Write-Output "Single-buffer diagnostic exit code: $singleExitCode"

    if ($singleExitCode -eq 0) {
        & $PythonPath $diagnosticPath `
            --buffers 1000 `
            --timeout 1.0 `
            --report $stabilityReportPath
        $exitCode = $LASTEXITCODE
        Write-Output "1000-buffer diagnostic exit code: $exitCode"
    }
    else {
        $exitCode = $singleExitCode
        Write-Output "Skipping 1000-buffer diagnostic because single-buffer acquisition failed."
    }
}
catch {
    $exitCode = 1
    Write-Error ($_ | Out-String)
}
finally {
    if ($serviceWasRunning) {
        Start-Service -Name $serviceName
        (Get-Service -Name $serviceName).WaitForStatus(
            [System.ServiceProcess.ServiceControllerStatus]::Running,
            [TimeSpan]::FromSeconds(20)
        )
    }
    Write-Output "Final $serviceName status: $((Get-Service -Name $serviceName).Status)"
    Stop-Transcript | Out-Null
}

exit $exitCode
