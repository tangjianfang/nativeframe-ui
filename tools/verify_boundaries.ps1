[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Root
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$rootPath = (Resolve-Path -LiteralPath $Root).Path
if (-not (Get-Item -LiteralPath $rootPath).PSIsContainer) {
    throw "Boundary-check root must be a directory: $rootPath"
}

$scanRoots = @('include', 'src', 'samples', 'tests', 'resources', 'cmake')
$sourceExtensions = @('.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx', '.inl', '.ixx', '.rc', '.cmake')
$violations = [System.Collections.Generic.List[string]]::new()
$files = [System.Collections.Generic.List[System.IO.FileInfo]]::new()

function Test-ScannableFile {
    param([Parameter(Mandatory = $true)][System.IO.FileInfo]$Item)

    if ($Item.Name -eq 'CMakeLists.txt') {
        return $true
    }
    return $sourceExtensions -contains $Item.Extension.ToLowerInvariant()
}

function Get-RelativePath {
    param([Parameter(Mandatory = $true)][string]$Path)

    $rootWithSeparator = $rootPath.TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
    $rootUri = [Uri]$rootWithSeparator
    $pathUri = [Uri]$Path
    return [Uri]::UnescapeDataString($rootUri.MakeRelativeUri($pathUri).ToString()).Replace('/', [System.IO.Path]::DirectorySeparatorChar)
}

function Add-Violation {
    param(
        [Parameter(Mandatory = $true)][System.IO.FileInfo]$Item,
        [Parameter(Mandatory = $true)][string]$Content,
        [Parameter(Mandatory = $true)][System.Text.RegularExpressions.Match]$Match,
        [Parameter(Mandatory = $true)][string]$Rule,
        [Parameter(Mandatory = $true)][string]$Message
    )

    $line = 1
    if ($Match.Index -gt 0) {
        $line += ([regex]::Matches($Content.Substring(0, $Match.Index), "`n")).Count
    }
    $relativePath = Get-RelativePath -Path $Item.FullName
    $violations.Add("${relativePath}:${line}: [$Rule] $Message")
}

function Add-RegexViolations {
    param(
        [Parameter(Mandatory = $true)][System.IO.FileInfo]$Item,
        [Parameter(Mandatory = $true)][string]$Content,
        [Parameter(Mandatory = $true)][regex]$Regex,
        [Parameter(Mandatory = $true)][string]$Rule,
        [Parameter(Mandatory = $true)][string]$Message
    )

    foreach ($match in $Regex.Matches($Content)) {
        Add-Violation -Item $Item -Content $Content -Match $match -Rule $Rule -Message $Message
    }
}

foreach ($relative in $scanRoots) {
    $path = Join-Path $rootPath $relative
    if (-not (Test-Path -LiteralPath $path)) {
        continue
    }

    foreach ($item in Get-ChildItem -LiteralPath $path -Recurse -File) {
        if (Test-ScannableFile -Item $item) {
            $files.Add($item)
        }
    }
}

$topLevelCMake = Join-Path $rootPath 'CMakeLists.txt'
if (Test-Path -LiteralPath $topLevelCMake) {
    $files.Add((Get-Item -LiteralPath $topLevelCMake))
}

$forbiddenBrandingRegex = [regex]::new('\b(?:BCGControlBar|BCGCBPro)[A-Za-z0-9_]*\b', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
$forbiddenIncludeRegex = [regex]::new('^[ \t]*#[ \t]*include[ \t]*[<"][ \t]*(?:afx|atl|wtl|bcg)[^>"\r\n]*[>"]', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase -bor [System.Text.RegularExpressions.RegexOptions]::Multiline)
$unfinishedWorkRegex = [regex]::new('(?://[^\r\n]*\b(?:TODO|FIXME|XXX)\b|/\*[\s\S]*?\b(?:TODO|FIXME|XXX)\b[\s\S]*?\*/|^[ \t]*#[^\r\n]*\b(?:TODO|FIXME|XXX)\b)', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase -bor [System.Text.RegularExpressions.RegexOptions]::Multiline)
$deprecatedApiRegex = [regex]::new('\b(?:GetVersion(?:Ex)?(?:A|W)?|IsBad(?:ReadPtr|WritePtr|CodePtr|StringPtr(?:A|W))|WinExec|LoadModule|strcpy|strcat|wcscpy|wcscat|sprintf|vsprintf|wsprintf(?:A|W)?|lstrcpy(?:A|W)?|lstrcat(?:A|W)?|gets|_getws)\s*\(')
$pointerLongApiRegex = [regex]::new('\b(?:GetWindowLong|SetWindowLong)(?:A|W)?\s*\([^,\r\n]+,\s*(?:GWL_USERDATA|GWL_WNDPROC|GWLP_USERDATA|GWLP_WNDPROC|DWL_DLGPROC|DWL_MSGRESULT|DWL_USER|DWLP_DLGPROC|DWLP_MSGRESULT|DWLP_USER)\b', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
$callbackRegex = [regex]::new('\b(?:LRESULT|INT_PTR|BOOL|DWORD|UINT|void)\s+CALLBACK\s+(?<name>[A-Za-z_][A-Za-z0-9_:]*)\s*\([^;{}]*\)\s*(?<suffix>[^;{}]*)[;{]', [System.Text.RegularExpressions.RegexOptions]::Singleline)
$hwndAccessorRegex = [regex]::new('\bHWND\s+hwnd\s*\([^;{}]*\)\s*(?<suffix>[^;{}]*)[;{]', [System.Text.RegularExpressions.RegexOptions]::Singleline)

$includeRoot = (Join-Path $rootPath 'include').TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
$srcRoot = (Join-Path $rootPath 'src').TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar

foreach ($item in $files) {
    $content = [string](Get-Content -LiteralPath $item.FullName -Raw)

    Add-RegexViolations -Item $item -Content $content -Regex $forbiddenBrandingRegex -Rule 'forbidden-dependency' -Message 'BCG branding or dependency marker is forbidden.'
    Add-RegexViolations -Item $item -Content $content -Regex $forbiddenIncludeRegex -Rule 'forbidden-include' -Message 'MFC, ATL, WTL, and BCG headers are forbidden in both angle-bracket and quoted includes.'
    Add-RegexViolations -Item $item -Content $content -Regex $unfinishedWorkRegex -Rule 'unfinished-work' -Message 'TODO, FIXME, and XXX comments must be resolved before merge.'
    Add-RegexViolations -Item $item -Content $content -Regex $deprecatedApiRegex -Rule 'deprecated-api' -Message 'Deprecated API call is forbidden; use its supported replacement.'
    Add-RegexViolations -Item $item -Content $content -Regex $pointerLongApiRegex -Rule 'pointer-api' -Message 'GetWindowLongPtr or SetWindowLongPtr is required for pointer-sized window data.'

    $isProductionCode = $item.FullName.StartsWith($includeRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
                        $item.FullName.StartsWith($srcRoot, [System.StringComparison]::OrdinalIgnoreCase)
    if ($isProductionCode) {
        foreach ($match in $callbackRegex.Matches($content)) {
            if ($match.Groups['suffix'].Value -notmatch '\bnoexcept\b') {
                Add-Violation -Item $item -Content $content -Match $match -Rule 'callback-noexcept' -Message "Win32 callback '$($match.Groups['name'].Value)' must be declared noexcept."
            }
        }
    }

    if ($item.FullName.StartsWith($includeRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        foreach ($match in $hwndAccessorRegex.Matches($content)) {
            if ($match.Groups['suffix'].Value -notmatch '\bnoexcept\b') {
                Add-Violation -Item $item -Content $content -Match $match -Rule 'hwnd-noexcept' -Message 'Public HWND accessors must be declared noexcept.'
            }
        }
    }
}

# The allow-list mirrors docs/ARCHITECTURE.md. Dependencies may point only
# downward; a new nfui_* target must receive an explicit policy entry.
$allowedModuleDependencies = @{
    nfui_core         = @()
    nfui_command      = @('nfui_core')
    nfui_layout       = @('nfui_core')
    nfui_theme        = @('nfui_core')
    nfui_window       = @('nfui_core')
    nfui_dialog       = @('nfui_core')
    nfui_control_base = @('nfui_core', 'nfui_theme')
    nfui_button       = @('nfui_core', 'nfui_theme', 'nfui_control_base')
    nfui_checkbox     = @('nfui_core', 'nfui_theme', 'nfui_control_base')
    nfui_radio        = @('nfui_core', 'nfui_theme', 'nfui_control_base')
    nfui_text         = @('nfui_core', 'nfui_theme', 'nfui_control_base')
    nfui_listbox      = @('nfui_core', 'nfui_theme', 'nfui_control_base')
    nfui_listview     = @('nfui_core', 'nfui_theme', 'nfui_control_base')
    nfui_treeview     = @('nfui_core', 'nfui_theme', 'nfui_control_base')
    nfui_iconview     = @('nfui_core', 'nfui_theme', 'nfui_control_base')
    nfui_frame        = @('nfui_core', 'nfui_theme', 'nfui_control_base')
    nfui_charts       = @('nfui_core', 'nfui_theme', 'nfui_window')
    nfui_charts_aa    = @('nfui_charts')
}

$cmakeRoot = Join-Path $rootPath 'cmake'
if (Test-Path -LiteralPath $cmakeRoot) {
    $moduleDeclarationRegex = [regex]::new('\badd_library\s*\(\s*(?<target>nfui_[A-Za-z0-9_]+)\b', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    $linkBlockRegex = [regex]::new('\btarget_link_libraries\s*\(\s*(?<target>nfui_[A-Za-z0-9_]+)\b(?<body>.*?)\)', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase -bor [System.Text.RegularExpressions.RegexOptions]::Singleline)
    $dependencyRegex = [regex]::new('(?<![A-Za-z0-9_:])(?:NativeFrameUI::)?(?<dependency>nfui_[A-Za-z0-9_]+)\b', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)

    foreach ($moduleFile in Get-ChildItem -LiteralPath $cmakeRoot -Filter 'nfui_*.cmake' -File) {
        $content = [string](Get-Content -LiteralPath $moduleFile.FullName -Raw)

        foreach ($declaration in $moduleDeclarationRegex.Matches($content)) {
            $target = $declaration.Groups['target'].Value.ToLowerInvariant()
            if (-not $allowedModuleDependencies.ContainsKey($target)) {
                Add-Violation -Item $moduleFile -Content $content -Match $declaration -Rule 'architecture-policy' -Message "Module '$target' has no dependency policy; update the ARCHITECTURE.md policy and checker together."
            }
        }

        foreach ($linkBlock in $linkBlockRegex.Matches($content)) {
            $target = $linkBlock.Groups['target'].Value.ToLowerInvariant()
            if (-not $allowedModuleDependencies.ContainsKey($target)) {
                continue
            }

            $allowed = @($allowedModuleDependencies[$target])
            foreach ($dependencyMatch in $dependencyRegex.Matches($linkBlock.Groups['body'].Value)) {
                $dependency = $dependencyMatch.Groups['dependency'].Value.ToLowerInvariant()
                if ($allowed -notcontains $dependency) {
                    Add-Violation -Item $moduleFile -Content $content -Match $linkBlock -Rule 'architecture-direction' -Message "Module '$target' may not depend on '$dependency'."
                }
            }
        }
    }
}

# Catch the most damaging direct-include shortcuts that can bypass a correct
# target_link_libraries graph. Window.cpp and Dialog.cpp are separate modules
# despite their historical location under src/core.
$coreForbiddenIncludeRegex = [regex]::new('^[ \t]*#[ \t]*include[ \t]*[<"][ \t]*nfui/(?:Controls(?:\.hpp|/)|Command\.hpp|Layout\.hpp|Theme\.hpp|Charts\.hpp)', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase -bor [System.Text.RegularExpressions.RegexOptions]::Multiline)
$lowerLayerForbiddenIncludeRegex = [regex]::new('^[ \t]*#[ \t]*include[ \t]*[<"][ \t]*nfui/(?:Controls(?:\.hpp|/)|Command\.hpp|Layout\.hpp|Theme\.hpp|Window\.hpp|Dialog\.hpp|Charts\.hpp|Persistence\.hpp)', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase -bor [System.Text.RegularExpressions.RegexOptions]::Multiline)
$controlIncludeRegex = [regex]::new('^[ \t]*#[ \t]*include[ \t]*[<"][ \t]*nfui/Controls(?:\.hpp|/)', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase -bor [System.Text.RegularExpressions.RegexOptions]::Multiline)
$persistenceWindowRegex = [regex]::new('(?:^[ \t]*#[ \t]*include[ \t]*[<"][ \t]*nfui/(?:Window|Dialog|Controls)(?:\.hpp|/)|\bHWND\b)', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase -bor [System.Text.RegularExpressions.RegexOptions]::Multiline)

foreach ($item in $files) {
    $relativePath = (Get-RelativePath -Path $item.FullName).Replace('/', '\')
    $content = [string](Get-Content -LiteralPath $item.FullName -Raw)

    if ($relativePath -like 'src\core\*' -and $item.Name -notin @('Window.cpp', 'Dialog.cpp')) {
        Add-RegexViolations -Item $item -Content $content -Regex $coreForbiddenIncludeRegex -Rule 'architecture-include' -Message 'Core source may not include controls, command, layout, theme, or charts.'
    }
    elseif ($relativePath -like 'src\dpi\*' -or $relativePath -like 'src\font\*' -or $relativePath -like 'src\icon\*' -or $relativePath -like 'src\paint\*' -or $relativePath -like 'src\resource\*') {
        Add-RegexViolations -Item $item -Content $content -Regex $lowerLayerForbiddenIncludeRegex -Rule 'architecture-include' -Message 'Lower-layer source may not include higher application or UI modules.'
    }
    elseif ($relativePath -like 'src\command\*' -or $relativePath -like 'src\layout\*') {
        Add-RegexViolations -Item $item -Content $content -Regex $controlIncludeRegex -Rule 'architecture-include' -Message 'Command and layout modules may not include concrete controls.'
    }
    elseif ($relativePath -like 'src\persistence\*' -or $relativePath -eq 'include\nfui\Persistence.hpp') {
        Add-RegexViolations -Item $item -Content $content -Regex $persistenceWindowRegex -Rule 'architecture-include' -Message 'Persistence may not depend on HWND, Window, Dialog, or controls.'
    }
}

if ($violations.Count -gt 0) {
    foreach ($violation in $violations) {
        [Console]::Error.WriteLine($violation)
    }
    [Console]::Error.WriteLine("NativeFrame UI boundary check failed with $($violations.Count) violation(s).")
    exit 1
}

Write-Output "NativeFrame UI dependency, architecture, and source-quality checks passed ($($files.Count) files scanned)."
