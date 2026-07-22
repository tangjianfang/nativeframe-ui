#requires -Version 5.1
<#
.SYNOPSIS
    Verifies the layered dependency direction described in docs/ARCHITECTURE.md.

.DESCRIPTION
    Scans every public header under include/nfui/**.hpp, extracts each
    `#include <nfui/...>` edge, maps both endpoints to their owning module,
    and fails if an edge violates the documented dependency direction.

    Two complementary rule sets are enforced (both mirror the
    "Detailed dependency rules" / "Forbidden dependencies" blocks in
    docs/ARCHITECTURE.md):

      1. Per-module allow-list. A header may only include headers from its own
         module or from a lower module it is explicitly allowed to depend on.
         This catches, e.g., core -> controls / command / theme / layout.

      2. Per-module named-forbidden patterns. These express rules that are
         finer-grained than the module allow-list, most importantly
         "persistence -> HWND / Window objects": Window.hpp lives in the core
         module (which persistence may otherwise depend on), so the specific
         Window/Dialog dependency must be forbidden explicitly.

    The umbrella header (NativeFrameUI.hpp) aggregates the whole surface and is
    exempt by design.
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# --- Module resolution ------------------------------------------------------
# Map a header path relative to include/nfui (e.g. "Controls/Button.hpp" or
# "Theme.hpp") to its owning module.
function Get-ModuleName {
    param([string]$Relative)

    $normalized = $Relative -replace '\\', '/'

    if ($normalized -eq 'Controls.hpp' -or $normalized -like 'Controls/*') {
        return 'controls'
    }

    $leaf = Split-Path $normalized -Leaf
    switch ($leaf) {
        'Diagnostics.hpp'     { return 'core' }
        'Handle.hpp'          { return 'core' }
        'Window.hpp'          { return 'core' }
        'Application.hpp'     { return 'core' }
        'HoverState.hpp'      { return 'core' }
        'Dialog.hpp'          { return 'core' }
        'Dpi.hpp'             { return 'dpi' }
        'Theme.hpp'           { return 'theme' }
        'Font.hpp'            { return 'font' }
        'Icon.hpp'            { return 'icon' }
        'Paint.hpp'           { return 'paint' }
        'ResourceContext.hpp' { return 'resource' }
        'Command.hpp'         { return 'command' }
        'Layout.hpp'          { return 'layout' }
        'Persistence.hpp'     { return 'persistence' }
        'Charts.hpp'          { return 'charts' }
        'NativeFrameUI.hpp'   { return 'umbrella' }
        default               { return 'unknown' }
    }
}

# --- Rule set ---------------------------------------------------------------
# Allowed cross-module dependencies. A module may always include its own
# headers; the sets below list which *other* modules it may depend on.
# Lower layers are kept strict (they carry the forbidden rules); higher
# layers (controls / charts) are permitted the low-level utility modules.
$allowed = @{
    'core'        = @()
    'dpi'         = @('core')
    'font'        = @('core')
    'icon'        = @('core')
    'theme'       = @('core', 'dpi')
    'paint'       = @('core', 'theme')
    'resource'    = @('core')
    'command'     = @('core')
    'layout'      = @('core', 'dpi')
    'persistence' = @('core', 'theme')
    'controls'    = @('core', 'dpi', 'theme', 'font', 'icon', 'paint')
    'charts'      = @('core', 'dpi', 'theme', 'font', 'icon', 'paint', 'controls')
    'umbrella'    = @('core', 'dpi', 'font', 'icon', 'theme', 'paint', 'resource', 'command', 'layout', 'persistence', 'controls', 'charts')
}

# Named-forbidden include patterns (matched against the target header relative
# path). These express doc rules finer than the module allow-list.
$forbiddenHeaders = @{
    # "persistence -> HWND / Window objects" — Window/Dialog own the HWND.
    'persistence' = @('Window.hpp', 'Dialog.hpp')
    # "command -> Menu / Toolbar / TreeView"
    'command'     = @('Controls/')
    # "theme -> business modules"
    'theme'       = @('Controls/', 'Charts.hpp', 'Command.hpp', 'Layout.hpp', 'Persistence.hpp')
    # "resource -> business modules"
    'resource'    = @('Controls/', 'Charts.hpp', 'Command.hpp', 'Layout.hpp', 'Persistence.hpp', 'Theme.hpp')
    # "layout -> concrete control implementations"
    'layout'      = @('Controls/', 'Charts.hpp')
    # "core -> controls / command" (and the rest of the upper layers)
    'core'        = @('Controls/', 'Charts.hpp', 'Command.hpp', 'Layout.hpp', 'Persistence.hpp')
}

# --- Scan -------------------------------------------------------------------
$includeRoot = Join-Path $Root 'include/nfui'
if (-not (Test-Path $includeRoot)) {
    Write-Error "include/nfui not found under '$Root'."
    exit 2
}
$includeRoot = (Resolve-Path $includeRoot).Path

$includeRegex = [regex]'#include\s*<nfui/([A-Za-z0-9_/]+\.hpp)>'
$violations = @()
$edgeCount = 0

$headers = Get-ChildItem -Path $includeRoot -Recurse -File -Filter '*.hpp'
foreach ($header in $headers) {
    $relative = $header.FullName.Substring($includeRoot.Length).TrimStart('\', '/') -replace '\\', '/'
    $srcModule = Get-ModuleName $relative

    if ($srcModule -eq 'umbrella') { continue }   # aggregation header is exempt

    if ($srcModule -eq 'unknown') {
        $violations += "UNMAPPED HEADER: $relative is not assigned to a module (update verify_architecture.ps1)."
        continue
    }

    $lines = Get-Content -Path $header.FullName
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $match = $includeRegex.Match($lines[$i])
        if (-not $match.Success) { continue }

        $targetRel = $match.Groups[1].Value
        if ($targetRel -eq $relative) { continue }   # self include

        $tgtModule = Get-ModuleName $targetRel
        $edgeCount++
        $lineNo = $i + 1

        if ($tgtModule -eq 'unknown') {
            $violations += "$relative`:$lineNo -> nfui/$targetRel : target header is not mapped to a module."
            continue
        }

        # Rule 1: module allow-list (intra-module always allowed).
        if ($tgtModule -ne $srcModule -and ($allowed[$srcModule] -notcontains $tgtModule)) {
            $violations += "$relative`:$lineNo : '$srcModule' must not depend on '$tgtModule' (include <nfui/$targetRel>)."
        }

        # Rule 2: named-forbidden header patterns for this module.
        if ($forbiddenHeaders.ContainsKey($srcModule)) {
            foreach ($pattern in $forbiddenHeaders[$srcModule]) {
                if ($targetRel -like "*$pattern*") {
                    $violations += "$relative`:$lineNo : '$srcModule' is forbidden to include <nfui/$targetRel> (pattern '$pattern')."
                }
            }
        }
    }
}

if ($violations.Count -gt 0) {
    Write-Host "NativeFrame UI architecture dependency check FAILED:" -ForegroundColor Red
    $violations | Sort-Object -Unique | ForEach-Object { Write-Error $_ }
    exit 1
}

Write-Output "NativeFrame UI architecture dependency check passed ($edgeCount nfui edge(s) across $($headers.Count) header(s))."
