param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

$forbidden = @('BCGControlBar', '#include <afx', '#include <atl', '#include <wtl', 'BCGCBPro')
$paths = @('include', 'src', 'samples', 'tests', 'resources', 'cmake', 'CMakeLists.txt')
$violations = @()

foreach ($relative in $paths) {
    $path = Join-Path $Root $relative
    if (-not (Test-Path $path)) {
        continue
    }

    $items = if ((Get-Item $path).PSIsContainer) {
        Get-ChildItem -Path $path -Recurse -File
    } else {
        Get-Item $path
    }

    foreach ($item in $items) {
        $content = Get-Content -Path $item.FullName -Raw -ErrorAction Stop
        foreach ($marker in $forbidden) {
            if ($content -like "*$marker*") {
                $violations += "$($item.FullName): forbidden marker '$marker'"
            }
        }
    }
}

if ($violations.Count -gt 0) {
    $violations | ForEach-Object { Write-Error $_ }
    exit 1
}

Write-Output "NativeFrame UI dependency boundary check passed."
