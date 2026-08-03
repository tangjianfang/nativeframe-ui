<#
.SYNOPSIS
    Per-pixel RGB diff scorer for the visual audit.

.DESCRIPTION
    Compares every audit PNG in $AuditOutput against the matching baseline
    in $Baseline and reports a per-pixel diff score. Replaces the
    byte-size heuristic in gate.ps1 with a real RGB-channel comparison.

    Score semantics:
      * perPixelMax    - largest absolute channel diff across the whole
                          file (0..255). 0 = identical, 255 = max distance.
      * perPixelMean   - average absolute channel diff over every pixel
                          (0..255).
      * diffPixelsPct  - share of pixels with ANY channel diff > $Threshold.

    Exit codes:
      0  every (demo × theme) capture is within tolerance
      1  at least one capture exceeds tolerance
      2  bad / missing parameters (StrictMode / Resolve-Path failure)
#>
[CmdletBinding()]
param(
    [string]$Root = '',
    [string]$AuditOutput = '',
    [string]$Baseline = '',
    [int]$PixelThreshold = 12,           # any-channel diff above this counts as "diff pixel"
    [int]$MaxMean = 4,                   # per-file mean channel diff ceiling
    [int]$MaxPct = 5,                    # per-file diff-pixel share ceiling (% * 100, so 500 = 5%)
    [string[]]$DemoList = @('Workbench','Showcase','DarkStudio','SettingsDemo','ResourceGallery',
                            'ThemeDemo','ComponentGallery','Controls','Charts','ChartsInteractive',
                            'Minimal','ControlsPlayground','IconGallery','DialogTour')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Same launch-context hardening as run_audit.ps1 (CP33): $PSScriptRoot can
# be empty when the script is invoked through a shell that does not
# preserve the launch context, which used to collapse the default -Root
# to "C:\..\..". Resolve from $PSCommandPath instead.
$script:ScorerDir = if (![string]::IsNullOrWhiteSpace($PSScriptRoot)) {
    $PSScriptRoot
} elseif (![string]::IsNullOrWhiteSpace($PSCommandPath)) {
    Split-Path -Parent $PSCommandPath
} else {
    $PWD.Path
}
if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = (Resolve-Path (Join-Path $script:ScorerDir '..\..')).Path
}
if ([string]::IsNullOrWhiteSpace($AuditOutput)) {
    $AuditOutput = Join-Path $Root 'docs\VISUAL_AUDIT'
}
if ([string]::IsNullOrWhiteSpace($Baseline)) {
    $Baseline = Join-Path $Root 'docs\VISUAL_AUDIT\baseline'
}

Add-Type -AssemblyName System.Drawing

# CP-B20 perf: GetPixel on a per-pixel basis allocates a Color struct
# every call. Across 42 PNGs at ~800k pixels each that's ~33M managed
# allocations per run, which thrashes the GC. Moving the diff loop
# itself into compiled C# is the actual win — PowerShell 5.1's foreach
# is ~10x slower than PowerShell 7 for tight byte loops, and we still
# want the gate to fit under a 60s CTest budget on whichever runtime
# the CI image happens to ship with. The C# helper does LockBits +
# Marshal.Copy + the per-pixel diff in native code and returns the
# summary stats as a PSCustomObject — no per-pixel managed objects
# cross the language boundary.
$script:DiffHelperAssembly = @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class NfuiDiffHelper
{
    public static object DiffPngs(string auditPath, string basePath, int pixelThreshold)
    {
        using (var auditImg = new Bitmap(auditPath))
        using (var baseImg  = new Bitmap(basePath))
        {
            if (auditImg.Width != baseImg.Width || auditImg.Height != baseImg.Height)
            {
                return new {
                    Width = auditImg.Width, Height = auditImg.Height,
                    BaseWidth = baseImg.Width, BaseHeight = baseImg.Height,
                    SizeMismatch = true, Max = 255, Mean = 255, DiffPctX100 = 10000
                };
            }
            int w = auditImg.Width, h = auditImg.Height;
            long totalPixels = (long)w * h;
            Bitmap a32 = auditImg.PixelFormat == PixelFormat.Format32bppArgb
                ? auditImg
                : auditImg.Clone(new Rectangle(0, 0, w, h), PixelFormat.Format32bppArgb);
            Bitmap b32 = baseImg.PixelFormat == PixelFormat.Format32bppArgb
                ? baseImg
                : baseImg.Clone(new Rectangle(0, 0, w, h), PixelFormat.Format32bppArgb);
            bool disposeA = (a32 != auditImg);
            bool disposeB = (b32 != baseImg);
            try
            {
                var rect = new Rectangle(0, 0, w, h);
                var aData = a32.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
                var bData = b32.LockBits(rect, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
                int bufSize = aData.Stride * h;
                byte[] aBuf = new byte[bufSize];
                byte[] bBuf = new byte[bufSize];
                try
                {
                    Marshal.Copy(aData.Scan0, aBuf, 0, bufSize);
                    Marshal.Copy(bData.Scan0, bBuf, 0, bufSize);
                }
                finally
                {
                    a32.UnlockBits(aData);
                    b32.UnlockBits(bData);
                }
                int maxDiff = 0;
                long sumDiff = 0;
                long diffPixels = 0;
                for (int i = 0; i < bufSize; i += 4)
                {
                    int db = Math.Abs(aBuf[i]     - bBuf[i]);
                    int dg = Math.Abs(aBuf[i + 1] - bBuf[i + 1]);
                    int dr = Math.Abs(aBuf[i + 2] - bBuf[i + 2]);
                    int localMax = dr > dg ? dr : dg;
                    if (db > localMax) localMax = db;
                    sumDiff += (dr + dg + db);
                    if (localMax > maxDiff) maxDiff = localMax;
                    if (localMax > pixelThreshold) diffPixels++;
                }
                int meanDiff = (int)(sumDiff / (totalPixels * 3));
                long diffPctX100 = (diffPixels * 10000L) / totalPixels;
                return new {
                    Width = w, Height = h,
                    BaseWidth = w, BaseHeight = h,
                    SizeMismatch = false, Max = maxDiff, Mean = meanDiff, DiffPctX100 = diffPctX100
                };
            }
            finally
            {
                if (disposeA) a32.Dispose();
                if (disposeB) b32.Dispose();
            }
        }
    }
}
'@
# Compile the helper. Add-Type -TypeDefinition chokes on the transitive
# GDI+ references under PowerShell 7 (System.Private.Windows.GdiPlus,
# System.Private.Windows.Core, etc.) — its inline compiler can't resolve
# them. Compiling via csc.exe resolves the framework reference graph
# transparently and produces a DLL we load once. The helper source
# lives in NfuiDiffHelper.cs so it's visible to unit tests and easy to
# read in isolation from the PowerShell plumbing.
$script:HelperSourcePath = Join-Path $PSScriptRoot 'NfuiDiffHelper.cs'
if (-not (Test-Path -LiteralPath $script:HelperSourcePath -PathType Leaf)) {
    throw "Required helper source not found: $script:HelperSourcePath"
}
$script:HelperDllPath = Join-Path ([System.IO.Path]::GetTempPath()) 'NfuiDiffHelper.dll'
$csc = (Get-Command csc.exe -ErrorAction SilentlyContinue)
if (-not $csc) {
    # No Roslyn compiler on PATH — try the well-known VS 2022 install path.
    $vsCsc = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\Roslyn\csc.exe"
    if (Test-Path -LiteralPath $vsCsc -PathType Leaf) {
        $csc = Get-Item $vsCsc
    }
}
if ($csc) {
    # Skip the rebuild if the DLL is newer than the source. csc.exe fails
    # when it can't overwrite a locked DLL — which is exactly what happens
    # on the second scorer invocation in a regression test, since PowerShell
    # holds the in-memory helper assembly open.
    $needCompile = $true
    if (Test-Path -LiteralPath $script:HelperDllPath -PathType Leaf) {
        $srcTime = (Get-Item -LiteralPath $script:HelperSourcePath).LastWriteTimeUtc
        $dllTime = (Get-Item -LiteralPath $script:HelperDllPath).LastWriteTimeUtc
        if ($dllTime -ge $srcTime) {
            $needCompile = $false
        }
    }
    if ($needCompile) {
        & $csc -nologo -target:library -out:$script:HelperDllPath $script:HelperSourcePath 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to compile NfuiDiffHelper.cs (exit $LASTEXITCODE). Is the .NET Framework SDK installed?"
        }
    }
    Add-Type -Path $script:HelperDllPath
} else {
    # Fallback: Add-Type with the single System.Drawing reference. This
    # works under PS 5.1 only; PS 7 will fail with the GDI+ reference
    # error and the user should install .NET Framework SDK or run pwsh -
    # Configuration $Env:PSModulePath + DevTools.
    Add-Type -ReferencedAssemblies 'System.Drawing' -TypeDefinition $script:DiffHelperAssembly -Language CSharp
}

# CP-B20: the scorer mirrors the (demo × theme) matrix from gate.ps1 so a
# missing file trips before the diff phase even starts. New demos / themes
# only need to be added here AND in run_audit.ps1.
$demos = if ($DemoList) { $DemoList } else {
    @('Workbench','Showcase','DarkStudio','SettingsDemo','ResourceGallery',
      'ThemeDemo','ComponentGallery','Controls','Charts','ChartsInteractive',
      'Minimal','ControlsPlayground','IconGallery','DialogTour')
}
$themes = @('light','dark','hc')

# Stage A: verify both sides have every file.
$missing = New-Object System.Collections.Generic.List[string]
foreach ($d in $demos) {
    foreach ($t in $themes) {
        $a = Join-Path $AuditOutput ("{0}_{1}.png" -f $d, $t)
        $b = Join-Path $Baseline   ("{0}_{1}.png" -f $d, $t)
        if (-not (Test-Path -LiteralPath $a -PathType Leaf)) {
            $missing.Add("audit missing: $a")
        }
        if (-not (Test-Path -LiteralPath $b -PathType Leaf)) {
            $missing.Add("baseline missing: $b")
        }
    }
}
if ($missing.Count -gt 0) {
    Write-Host ""
    Write-Host "FAILED: required files missing:"
    foreach ($m in $missing) { Write-Host "  $m" }
    exit 1
}

# Stage B: per-pixel diff using the compiled C# helper above. The naive
# GetPixel path allocates a Color struct per pixel — across 42 captures
# that's ~33M managed objects per run, which turns a 10s job into a
# 20-minute one. LockBits + Marshal.Copy + a native byte-walk loop
# brings that back to single-digit seconds.
$pixelThreshold = $PixelThreshold

function Compare-PixelDiff([string]$AuditPath, [string]$BaselinePath) {
    # The actual diff lives in compiled C# (NfuiDiffHelper.DiffPngs) so the
    # per-pixel loop runs in CLR, not in PowerShell 5.1's 10x-slower
    # foreach. The C# helper returns a plain object with the summary
    # stats — only one allocation crosses the language boundary per
    # capture, regardless of image size.
    $result = [NfuiDiffHelper]::DiffPngs($AuditPath, $BaselinePath, $script:pixelThreshold)
    return [pscustomobject]@{
        Width = [int]$result.Width
        Height = [int]$result.Height
        BaseWidth = [int]$result.BaseWidth
        BaseHeight = [int]$result.BaseHeight
        SizeMismatch = [bool]$result.SizeMismatch
        Max = [int]$result.Max
        Mean = [int]$result.Mean
        DiffPctX100 = [int]$result.DiffPctX100
    }
}

$violations = New-Object System.Collections.Generic.List[object]
$rows = New-Object System.Collections.Generic.List[object]
foreach ($d in $demos) {
    foreach ($t in $themes) {
        $audit = Join-Path $AuditOutput ("{0}_{1}.png" -f $d, $t)
        $base  = Join-Path $Baseline   ("{0}_{1}.png" -f $d, $t)
        $diff = Compare-PixelDiff -AuditPath $audit -BaselinePath $base
        $row = [pscustomobject]@{
            Demo = $d
            Theme = $t
            Width = $diff.Width
            Height = $diff.Height
            Max = $diff.Max
            Mean = $diff.Mean
            Pct = $diff.DiffPctX100 / 100.0
            SizeMismatch = $diff.SizeMismatch
        }
        $rows.Add($row)
        if ($diff.SizeMismatch -or
            $diff.Mean -gt $MaxMean -or
            $diff.DiffPctX100 -gt $MaxPct) {
            $violations.Add($row)
        }
    }
}

Write-Host "Visual-audit per-pixel diff (CP-B20):"
Write-Host ("  audit output  : {0}" -f $AuditOutput)
Write-Host ("  baseline      : {0}" -f $Baseline)
Write-Host ("  thresholds    : mean <= {0} ch, diff-pixels <= {1}%, pixel-threshold = {2}" -f $MaxMean, ($MaxPct/100.0), $PixelThreshold)
Write-Host ""
Write-Host ("  {0,-22} {1,-8} {2,-9} {3,-9} {4,-9}" -f 'demo_theme','max','mean','diff%','size')
foreach ($r in $rows) {
    $label = ("{0}_{1}" -f $r.Demo, $r.Theme)
    $size = ("{0}x{1}" -f $r.Width, $r.Height)
    Write-Host ("  {0,-22} {1,-8} {2,-9} {3,-9:F2} {4,-9}" -f $label, $r.Max, $r.Mean, $r.Pct, $size)
}

if ($violations.Count -gt 0) {
    Write-Host ""
    Write-Host "FAILED: $($violations.Count) capture(s) exceeded tolerance:"
    foreach ($v in $violations) {
        $label = ("{0}_{1}" -f $v.Demo, $v.Theme)
        Write-Host ("  {0,-22} max={1} mean={2} diff%={3:F2}" -f $label, $v.Max, $v.Mean, $v.Pct)
    }
    exit 1
}

Write-Host ""
Write-Host "PASS: every capture is within tolerance."
exit 0