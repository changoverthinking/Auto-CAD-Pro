param()

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $repoRoot "src\app\main_win.cpp"
$content = Get-Content -Raw -Path $sourcePath

function Fail([string]$message) {
    Write-Error $message
    exit 1
}

function UniqueMatches([string]$pattern) {
    $set = New-Object 'System.Collections.Generic.HashSet[string]'
    foreach ($m in [regex]::Matches($content, $pattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)) {
        [void]$set.Add($m.Groups[1].Value)
    }
    return @($set)
}

$menuConstants = UniqueMatches 'constexpr\s+int\s+(kMenu[A-Za-z0-9_]+)\s*='
$menuEntries = UniqueMatches 'AppendMenuW\([^;]*?\b(kMenu[A-Za-z0-9_]+)\b'
$menuHandlers = UniqueMatches 'case\s+(kMenu[A-Za-z0-9_]+)\s*:'

foreach ($id in $menuConstants) {
    if ($menuEntries -notcontains $id) { Fail "Menu command is declared but never exposed: $id" }
    if ($menuHandlers -notcontains $id) { Fail "Menu command is declared but has no WM_COMMAND handler: $id" }
}
foreach ($id in $menuEntries) {
    if ($menuConstants -notcontains $id) { Fail "Menu entry references undeclared command: $id" }
    if ($menuHandlers -notcontains $id) { Fail "Menu entry has no WM_COMMAND handler: $id" }
}

$toolConstants = UniqueMatches 'constexpr\s+int\s+(kTool[A-Za-z0-9_]+)\s*='
$toolHandlers = UniqueMatches 'case\s+(kTool[A-Za-z0-9_]+)\s*:'
foreach ($id in $toolConstants) {
    if ($toolHandlers -notcontains $id) { Fail "Toolbar command has no WM_COMMAND handler: $id" }
}

$toolEnum = [regex]::Match(
    $content,
    'enum\s+class\s+Tool\s*\{(?<body>.*?)\};',
    [System.Text.RegularExpressions.RegexOptions]::Singleline)
if (!$toolEnum.Success) { Fail "Could not locate Tool enum." }

$toolNames = @()
foreach ($part in ($toolEnum.Groups['body'].Value -split ',')) {
    $name = ($part -replace '//.*$', '').Trim()
    if ($name) { $toolNames += ($name -split '=')[0].Trim() }
}

$toolbarBlock = [regex]::Match(
    $content,
    'kToolbarButtons\s*\{(?<body>.*?)\n\};',
    [System.Text.RegularExpressions.RegexOptions]::Singleline)
$leftBlock = [regex]::Match(
    $content,
    'kLeftRailTools\s*\{(?<body>.*?)\n\};',
    [System.Text.RegularExpressions.RegexOptions]::Singleline)

if (!$toolbarBlock.Success -or !$leftBlock.Success) {
    Fail "Could not locate toolbar or left-rail tool definitions."
}

$reachable = New-Object 'System.Collections.Generic.HashSet[string]'
$reachText = $toolbarBlock.Groups['body'].Value + [Environment]::NewLine + $leftBlock.Groups['body'].Value
foreach ($m in [regex]::Matches($reachText, 'Tool::([A-Za-z0-9_]+)')) {
    [void]$reachable.Add($m.Groups[1].Value)
}

foreach ($tool in $toolNames) {
    if (!$reachable.Contains($tool)) {
        Fail "Tool enum value is not reachable from toolbar or left rail: $tool"
    }
}

$leftTools = @([regex]::Matches($leftBlock.Groups['body'].Value, 'Tool::([A-Za-z0-9_]+)') |
    ForEach-Object { $_.Groups[1].Value })
if (($leftTools | Select-Object -Unique).Count -ne $leftTools.Count) {
    Fail "Left tool rail contains duplicate entries."
}

Write-Host ("GUI reachability checks passed: {0} menu commands, {1} tool commands, {2} Tool enum values." -f
    $menuConstants.Count, $toolConstants.Count, $toolNames.Count)
