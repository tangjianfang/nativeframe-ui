<#
.SYNOPSIS
    Self-test for tools/visual_audit/diff_scorer.ps1.

.DESCRIPTION
    Creates an isolated fixture under a temporary directory: a fake
    baseline, an identical audit PNG (expected PASS), and a corrupted
    audit PNG (expected FAIL). Invokes the scorer with -AuditOutput /
    -Baseline pointing at the fixture and asserts the exit code matches.

    Mirrors the verify_boundaries_tests.ps1 pattern so the diff scorer
    has its own regression coverage that runs on every CTest.
#>
[CmdletBinding()]
param(
    [string]$ScriptPath = ''
)
if ([string]::IsNullOrWhiteSpace($ScriptPath)) {
    $ScriptPath = if ($PSScriptRoot) { Join-Path $PSScriptRoot '..\tools\visual_audit\diff_scorer.ps1' } else { 'tools/visual_audit/diff_scorer.ps1' }
}

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

$fixtureRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("nfui_diff_scorer_fixture_" + [guid]::NewGuid().ToString('N'))
$audit = Join-Path $fixtureRoot 'audit'
$base  = Join-Path $fixtureRoot 'baseline'
New-Item -ItemType Directory -Path $audit -Force | Out-Null
New-Item -ItemType Directory -Path $base  -Force | Out-Null

# Stage 1: write a 100x100 solid-fill PNG as the baseline reference.
function New-SolidPng([string]$Path, [System.Drawing.Color]$Color) {
    $bmp = New-Object System.Drawing.Bitmap 100, 100
    try {
        for ($y = 0; $y -lt 100; $y++) {
            for ($x = 0; $x -lt 100; $x++) {
                $bmp.SetPixel($x, $y, $Color)
            }
        }
        $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $bmp.Dispose()
    }
}
New-SolidPng -Path (Join-Path $base 'Fixture_light.png')  -Color ([System.Drawing.Color]::FromArgb(255, 10, 20, 30))
New-SolidPng -Path (Join-Path $audit 'Fixture_light.png') -Color ([System.Drawing.Color]::FromArgb(255, 10, 20, 30))
# Corrupted copy — every pixel nudged by 50 channels.
New-SolidPng -Path (Join-Path $audit 'Fixture_dark.png')  -Color ([System.Drawing.Color]::FromArgb(255, 60, 70, 80))
# Same as baseline for dark — should PASS.
New-SolidPng -Path (Join-Path $base  'Fixture_dark.png')  -Color ([System.Drawing.Color]::FromArgb(255, 60, 70, 80))
# hc missing on the audit side — Stage A should trip before pixel diff.
# (We write only the baseline hc.)

# But wait — the scorer needs (Fixture, hc) on BOTH sides. We'll add a
# third scenario where the audit hc is present but the baseline is
# corrupted differently — that's covered below by a 100% pixel diff
# entry which still trips Stage A's missing check.
#
# Build the additional fixtures for hc:
New-SolidPng -Path (Join-Path $base  'Fixture_hc.png') -Color ([System.Drawing.Color]::FromArgb(255, 0, 0, 0))
# Audit side: deliberately do NOT create Fixture_hc.png so Stage A fails.

$root = $fixtureRoot

# Sub-test 1: identical PNGs should PASS (max=0, mean=0). Stage A only
# flags files where EITHER side is missing, so we need both sides to be
# present. Add a same-content png for the "identical" case.
# (Already done above with Fixture_light.)

# Test A: identical PNGs pass the scorer (exit 0).
& $ScriptPath -Root $root -AuditOutput $audit -Baseline $base -DemoList @('Fixture') 2>&1 | Out-Null
$exitA = $LASTEXITCODE
# Stage A flags the missing hc, so Test A is EXPECTED to FAIL with the
# current fixture set. Use that as Test B instead — see below.

# Test B: missing-file trip.
if ($exitA -eq 1) {
    Write-Host "PASS: missing-fixture path trips Stage A (exit 1)."
} else {
    Write-Error "Expected Stage A to flag the missing Fixture_hc.png on the audit side. Got exit=$exitA"
    exit 2
}

# Test C: build a fully populated fixture set so Stage A passes, then
# verify the diff scorer returns 0 for identical pairs and 1 for the
# corrupted pair. Use a second fixture root to keep the assertions
# independent.
$fixture2 = Join-Path ([System.IO.Path]::GetTempPath()) ("nfui_diff_scorer_fixture2_" + [guid]::NewGuid().ToString('N'))
$audit2 = Join-Path $fixture2 'audit'
$base2  = Join-Path $fixture2 'baseline'
New-Item -ItemType Directory -Path $audit2 -Force | Out-Null
New-Item -ItemType Directory -Path $base2  -Force | Out-Null

New-SolidPng -Path (Join-Path $base2  'A_light.png') -Color ([System.Drawing.Color]::FromArgb(255, 50, 100, 150))
New-SolidPng -Path (Join-Path $audit2 'A_light.png') -Color ([System.Drawing.Color]::FromArgb(255, 50, 100, 150))
New-SolidPng -Path (Join-Path $base2  'A_dark.png')  -Color ([System.Drawing.Color]::FromArgb(255, 20, 30, 40))
# Audit dark is heavily corrupted — every pixel nudged 100 channels.
New-SolidPng -Path (Join-Path $audit2 'A_dark.png')  -Color ([System.Drawing.Color]::FromArgb(255, 120, 130, 140))
New-SolidPng -Path (Join-Path $base2  'A_hc.png')    -Color ([System.Drawing.Color]::FromArgb(255, 0, 0, 0))
New-SolidPng -Path (Join-Path $audit2 'A_hc.png')    -Color ([System.Drawing.Color]::FromArgb(255, 0, 0, 0))

# Stage B: corrupted A_dark.png must trip the gate (mean > 4 or diff% > 0.05%).
& $ScriptPath -Root $fixture2 -AuditOutput $audit2 -Baseline $base2 -DemoList @('A') 2>&1 | Out-Null
if ($LASTEXITCODE -eq 1) {
    Write-Host "PASS: corrupted PNG trips the diff gate (exit 1)."
} else {
    Write-Error "Expected the corrupted A_dark.png to trip the gate. Got exit=$LASTEXITCODE"
    exit 3
}

# Stage C: replace the corrupted audit with an identical copy and verify
# the gate now passes. (Use direct file copy to keep the test hermetic.)
Copy-Item (Join-Path $base2 'A_dark.png') (Join-Path $audit2 'A_dark.png') -Force
& $ScriptPath -Root $fixture2 -AuditOutput $audit2 -Baseline $base2 -DemoList @('A') 2>&1 | Out-Null
if ($LASTEXITCODE -eq 0) {
    Write-Host "PASS: identical PNG pair passes the diff gate (exit 0)."
} else {
    Write-Error "Expected identical pairs to pass the gate. Got exit=$LASTEXITCODE"
    exit 4
}

# Cleanup the fixture trees.
Remove-Item -Recurse -Force $fixtureRoot
Remove-Item -Recurse -Force $fixture2

Write-Host ""
Write-Host "All diff_scorer self-tests passed."
exit 0