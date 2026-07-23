[CmdletBinding()]
param(
    [string]$ScriptPath = (Join-Path $PSScriptRoot '..\tools\verify_boundaries.ps1')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
# Native child processes (verify_boundaries.ps1) intentionally exit non-zero for
# the rejection fixtures. We read $LASTEXITCODE explicitly below, so opt out of
# PowerShell 7.3+'s native-command/ErrorActionPreference integration to keep
# those non-zero exits from being treated as errors on newer pwsh builds.
if ($null -ne $PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$resolvedScript = (Resolve-Path -LiteralPath $ScriptPath).Path
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("nfui-boundary-tests-{0}" -f [guid]::NewGuid())
$failures = [System.Collections.Generic.List[string]]::new()
$passes = 0

function New-FixtureRoot {
    param([Parameter(Mandatory = $true)][string]$Name)

    $root = Join-Path $tempRoot $Name
    foreach ($relative in @('include', 'src', 'samples', 'tests', 'resources', 'cmake')) {
        New-Item -ItemType Directory -Path (Join-Path $root $relative) -Force | Out-Null
    }
    Set-Content -LiteralPath (Join-Path $root 'include\safe.hpp') -Value "#pragma once`n" -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $root 'CMakeLists.txt') -Value "cmake_minimum_required(VERSION 3.25)`n" -Encoding UTF8
    return $root
}

function Invoke-BoundaryCheck {
    param([Parameter(Mandatory = $true)][string]$Root)

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $output = & powershell -NoProfile -ExecutionPolicy Bypass -File $resolvedScript -Root $Root 2>&1
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

function Assert-RejectedSource {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$RelativePath,
        [Parameter(Mandatory = $true)][string]$Content
    )

    $root = New-FixtureRoot -Name $Name
    $path = Join-Path $root $RelativePath
    $parent = Split-Path -Parent $path
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
    Set-Content -LiteralPath $path -Value $Content -Encoding UTF8
    Assert-ExitCode -Name $Name -Expected 1 -Actual (Invoke-BoundaryCheck -Root $root)
}

try {
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

    $cleanRoot = New-FixtureRoot -Name 'clean-source'
    Assert-ExitCode -Name 'clean source is accepted' -Expected 0 -Actual (Invoke-BoundaryCheck -Root $cleanRoot)

    $styleRoot = New-FixtureRoot -Name 'window-style-access'
    Set-Content -LiteralPath (Join-Path $styleRoot 'src\style.cpp') -Value 'auto style = GetWindowLongW(hwnd, GWL_STYLE);' -Encoding UTF8
    Assert-ExitCode -Name 'GetWindowLong style access is accepted' -Expected 0 -Actual (Invoke-BoundaryCheck -Root $styleRoot)

    Assert-RejectedSource -Name 'BCGControlBar branding' -RelativePath 'src\bad.cpp' -Content 'const char* legacy = "BCGControlBar";'
    Assert-RejectedSource -Name 'BCGCBPro branding' -RelativePath 'src\bad.cpp' -Content 'const char* legacy = "BCGCBPro";'
    Assert-RejectedSource -Name 'angle afx include' -RelativePath 'include\bad.hpp' -Content '#include <afxwin.h>'
    Assert-RejectedSource -Name 'quoted afx include' -RelativePath 'include\bad.hpp' -Content '#include "afxwin.h"'
    Assert-RejectedSource -Name 'angle atl include' -RelativePath 'include\bad.hpp' -Content '#include <atlbase.h>'
    Assert-RejectedSource -Name 'quoted wtl include' -RelativePath 'include\bad.hpp' -Content '#include "wtl/atlapp.h"'
    Assert-RejectedSource -Name 'angle BCG include' -RelativePath 'include\bad.hpp' -Content '#include <BCGPRibbonBar.h>'
    Assert-RejectedSource -Name 'quoted BCG include' -RelativePath 'include\bad.hpp' -Content '#include "BCGPVisualManager.h"'
    Assert-RejectedSource -Name 'unfinished-work marker' -RelativePath 'src\bad.cpp' -Content '// TODO: remove the placeholder'
    Assert-RejectedSource -Name 'multiline unfinished-work marker' -RelativePath 'src\bad.cpp' -Content "/*`n  FIXME: remove the placeholder`n*/"
    Assert-RejectedSource -Name 'deprecated Win32 API' -RelativePath 'src\bad.cpp' -Content 'GetVersionExW(&version_info);'
    Assert-RejectedSource -Name 'pointer-sized Win32 API' -RelativePath 'src\bad.cpp' -Content 'auto value = GetWindowLongW(hwnd, GWLP_USERDATA);'
    Assert-RejectedSource -Name 'callback without noexcept' -RelativePath 'src\bad.cpp' -Content 'LRESULT CALLBACK bad_proc(HWND, UINT, WPARAM, LPARAM) { return 0; }'
    Assert-RejectedSource -Name 'HWND accessor without noexcept' -RelativePath 'include\bad.hpp' -Content 'struct Wrapper { HWND hwnd() const; };'
    Assert-RejectedSource -Name 'core source includes controls' -RelativePath 'src\core\bad.cpp' -Content '#include <nfui/Controls/Button.hpp>'
    Assert-RejectedSource -Name 'lower layer includes theme' -RelativePath 'src\dpi\bad.cpp' -Content '#include <nfui/Theme.hpp>'

    $cmakeRoot = New-FixtureRoot -Name 'reversed-cmake-dependency'
    Set-Content -LiteralPath (Join-Path $cmakeRoot 'cmake\nfui_core.cmake') -Encoding UTF8 -Value @'
add_library(nfui_core STATIC src/core.cpp)
target_link_libraries(nfui_core PUBLIC NativeFrameUI::nfui_theme)
'@
    Assert-ExitCode -Name 'reversed CMake dependency' -Expected 1 -Actual (Invoke-BoundaryCheck -Root $cmakeRoot)
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}

if ($failures.Count -gt 0) {
    $failures | ForEach-Object { Write-Error $_ }
    Write-Output "$passes boundary regression tests passed; $($failures.Count) failed."
    exit 1
}

Write-Output "$passes boundary regression tests passed."
exit 0
