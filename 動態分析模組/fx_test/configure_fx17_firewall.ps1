param(
    [string]$PythonPath = "C:\Users\HSIPL\AppData\Local\Programs\Python\Python39\python.exe"
)

$ErrorActionPreference = "Stop"
$allowRuleName = "FX17 Harvester Python UDP"
$cameraSubnet = "169.254.0.0/16"
$logPath = Join-Path $PSScriptRoot "configure_fx17_firewall.log"
$exitCode = 0

Start-Transcript -Path $logPath -Force | Out-Null
try {
    if (-not (Test-Path -LiteralPath $PythonPath -PathType Leaf)) {
        throw "Python executable not found: $PythonPath"
    }

    $pythonUdpBlocks = Get-NetFirewallRule `
        -DisplayName "python.exe" `
        -ErrorAction SilentlyContinue |
        Where-Object {
            $application = $_ | Get-NetFirewallApplicationFilter
            $port = $_ | Get-NetFirewallPortFilter
            $isInboundBlock = (
                $_.Direction -eq "Inbound" -and $_.Action -eq "Block"
            )
            $sameProgram = $application.Program -ieq $PythonPath
            $isUdp = $port.Protocol -eq "UDP" -or $port.Protocol -eq 17
            $isInboundBlock -and $sameProgram -and $isUdp
        }

    if ($pythonUdpBlocks) {
        $pythonUdpBlocks | Disable-NetFirewallRule
    }

    Get-NetFirewallRule `
        -DisplayName $allowRuleName `
        -ErrorAction SilentlyContinue |
        Remove-NetFirewallRule

    New-NetFirewallRule `
        -DisplayName $allowRuleName `
        -Description "Allow read-only FX17 GenTL/GigE Vision traffic for project Python." `
        -Direction Inbound `
        -Action Allow `
        -Enabled True `
        -Profile Public `
        -Program $PythonPath `
        -Protocol UDP `
        -RemoteAddress $cameraSubnet | Out-Null

    Write-Output "Disabled matching Python inbound UDP block rules: $($pythonUdpBlocks.Count)"
    Get-NetFirewallRule -DisplayName "python.exe", $allowRuleName |
        ForEach-Object {
            $application = $_ | Get-NetFirewallApplicationFilter
            $port = $_ | Get-NetFirewallPortFilter
            [pscustomobject]@{
                DisplayName = $_.DisplayName
                Enabled = $_.Enabled
                Direction = $_.Direction
                Action = $_.Action
                Profile = $_.Profile
                Program = $application.Program
                Protocol = $port.Protocol
                RemoteAddress = (
                    ($_ | Get-NetFirewallAddressFilter).RemoteAddress -join ","
                )
            }
        } |
        Format-Table -AutoSize
}
catch {
    $exitCode = 1
    Write-Error ($_ | Out-String)
}
finally {
    Stop-Transcript | Out-Null
}

exit $exitCode
