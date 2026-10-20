$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$qtDir = Split-Path -Parent $scriptDir
$bundleRoot = Split-Path -Parent (Split-Path -Parent $qtDir)
$manifestPath = Join-Path $bundleRoot 'BUNDLE_SHA256.csv'

Get-ChildItem -LiteralPath $bundleRoot -Recurse -File |
    Where-Object {
        $relative = $_.FullName.Substring($bundleRoot.Length + 1)
        $_.FullName -ne $manifestPath -and
        $relative -notmatch '^(\.venv|build|release)\\' -and
        $relative -notmatch '^(acquisition_logs|analysis_captures|capture_diagnostics|sample_records)\\' -and
        $relative -notmatch '(^|\\)__pycache__(\\|$)' -and
        $_.Extension -ne '.pyc'
    } |
    Sort-Object FullName |
    ForEach-Object {
        $hash = Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName
        [PSCustomObject]@{
            RelativePath = $_.FullName.Substring($bundleRoot.Length + 1)
            Length = $_.Length
            SHA256 = $hash.Hash
        }
    } | Export-Csv -LiteralPath $manifestPath -NoTypeInformation -Encoding UTF8

Write-Output "Updated bundle manifest: $manifestPath"
