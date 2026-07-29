[CmdletBinding()]
param(
    [string]$ScriptPath = (Join-Path $PSScriptRoot '..\tools\visual_audit\gate.ps1')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
# Native child processes (gate.ps1) intentionally exit non-zero for the
# rejection fixtures. We read $LASTEXITCODE explicitly below, so opt out of
# PowerShell 7.3+'s native-command/ErrorActionPreference integration to keep
# those non-zero exits from being treated as errors on newer pwsh builds.
if ($null -ne $PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$resolvedScript = (Resolve-Path -LiteralPath $ScriptPath).Path
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("nfui-gate-tests-{0}" -f [guid]::NewGuid())
$failures = [System.Collections.Generic.List[string]]::new()
$passes = 0

# The placeholder gate produces 30 PNGs (10 demos x 3 themes), each well
# above the 4 KB per-file floor. The fake-payload helper writes a stub
# PNG-sized file so the script's existence check + byte-size heuristic
# both pass.
$expectedFiles = @()
foreach ($d in @('Workbench','Showcase','DarkStudio','SettingsDemo','DialogTour',
                 'ResourceGallery','ComponentGallery','ThemeDemo','ControlsPlayground',
                 'Charts')) {
    foreach ($t in @('light','dark','hc')) {
        $expectedFiles += ("{0}_{1}.png" -f $d, $t)
    }
}

function New-StubAuditOutput {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [int]$BytesPerFile = 24576
    )

    New-Item -ItemType Directory -Path $Path -Force | Out-Null
    foreach ($name in $expectedFiles) {
        $bytes = New-Object byte[] $BytesPerFile
        for ($i = 0; $i -lt $BytesPerFile; $i++) { $bytes[$i] = 0xFF }
        [System.IO.File]::WriteAllBytes((Join-Path $Path $name), $bytes)
    }
}

function Invoke-Gate {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$AuditOutput
    )

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File $resolvedScript `
        -Root $Root -AuditOutput $AuditOutput 2>&1
    $exitCode = $LASTEXITCODE
    $ErrorActionPreference = $previousErrorActionPreference
    return [pscustomobject]@{
        ExitCode = $exitCode
        Output = ($output | Out-String)
    }
}

function Assert-ExitCode {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][int]$Expected,
        [Parameter(Mandatory = $true)]$Actual
    )

    if ($Actual.ExitCode -ne $Expected) {
        $failures.Add("${Name}: expected exit code $Expected, got $($Actual.ExitCode). Output: $($Actual.Output.Trim())")
        return
    }
    $script:passes++
}

try {
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

    # Fixture 1: 30 stub PNGs above the byte-size floor → gate exits 0.
    $root1 = Join-Path $tempRoot 'all_passing'
    New-Item -ItemType Directory -Path $root1 -Force | Out-Null
    $audit1 = Join-Path $root1 'docs\VISUAL_AUDIT'
    New-StubAuditOutput -Path $audit1 -BytesPerFile 24576
    Assert-ExitCode -Name 'all 30 PNGs present and above floor' `
        -Expected 0 `
        -Actual (Invoke-Gate -Root $root1 -AuditOutput $audit1)

    # Fixture 2: missing one PNG → gate exits 1.
    $root2 = Join-Path $tempRoot 'one_missing'
    New-Item -ItemType Directory -Path $root2 -Force | Out-Null
    $audit2 = Join-Path $root2 'docs\VISUAL_AUDIT'
    New-StubAuditOutput -Path $audit2 -BytesPerFile 24576
    Remove-Item -LiteralPath (Join-Path $audit2 'Workbench_dark.png') -Force
    Assert-ExitCode -Name 'one PNG missing' `
        -Expected 1 `
        -Actual (Invoke-Gate -Root $root2 -AuditOutput $audit2)

    # Fixture 3: every PNG present but below the per-file floor → gate
    # exits 1 (audit timed out before the window painted).
    $root3 = Join-Path $tempRoot 'all_below_per_file_floor'
    New-Item -ItemType Directory -Path $root3 -Force | Out-Null
    $audit3 = Join-Path $root3 'docs\VISUAL_AUDIT'
    New-StubAuditOutput -Path $audit3 -BytesPerFile 512
    Assert-ExitCode -Name 'all PNGs present but below per-file floor' `
        -Expected 1 `
        -Actual (Invoke-Gate -Root $root3 -AuditOutput $audit3)

    # Fixture 4: 29 PNGs (one missing) and the rest are tiny → still
    # exits 1, but for the missing-file reason (missing check runs
    # first).
    $root4 = Join-Path $tempRoot 'missing_and_small'
    New-Item -ItemType Directory -Path $root4 -Force | Out-Null
    $audit4 = Join-Path $root4 'docs\VISUAL_AUDIT'
    New-StubAuditOutput -Path $audit4 -BytesPerFile 2048
    Remove-Item -LiteralPath (Join-Path $audit4 'Charts_hc.png') -Force
    Assert-ExitCode -Name 'missing + small combine' `
        -Expected 1 `
        -Actual (Invoke-Gate -Root $root4 -AuditOutput $audit4)
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

if ($failures.Count -gt 0) {
    Write-Host "Visual-audit gate tests FAILED:" -ForegroundColor Red
    foreach ($f in $failures) { Write-Host "  $f" }
    exit 1
}

Write-Host "Visual-audit gate tests passed ($passes fixtures)."
exit 0
