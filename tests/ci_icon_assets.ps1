param()

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$manifestPath = Join-Path $repoRoot "assets\icons\suicacad\manifest.tsv"
$spritePath = Join-Path $repoRoot "assets\icons\suicacad\suicacad-icons-extended.svg"
$mainWinPath = Join-Path $repoRoot "src\app\main_win.cpp"

function Fail([string]$message) {
    Write-Error $message
    exit 1
}

if (!(Test-Path $manifestPath)) { Fail "SuicaCad icon manifest is missing." }
if (!(Test-Path $spritePath)) { Fail "SuicaCad icon sprite is missing." }

$manifest = Import-Csv -Delimiter ([char]9) -Path $manifestPath
if ($manifest.Count -lt 50) {
    Fail ("SuicaCad icon manifest must contain at least 50 icons; found " + $manifest.Count)
}

$duplicates = $manifest |
    Group-Object name |
    Where-Object { $_.Count -gt 1 }
if ($duplicates) {
    Fail ("Duplicate SuicaCad icon names: " + (($duplicates.Name) -join ", "))
}

$sprite = Get-Content -Raw -Path $spritePath
foreach ($row in $manifest) {
    $id = 'id="icon-' + $row.name + '"'
    if ($sprite -notlike ("*" + $id + "*")) {
        Fail ("Sprite is missing icon symbol: " + $row.name)
    }
}

$runtimeToolIcons = @(
    "select",
    "line",
    "polyline",
    "rectangle",
    "circle",
    "arc",
    "move",
    "copy",
    "rotate",
    "scale",
    "mirror",
    "trim",
    "extend",
    "offset",
    "dimension",
    "hatch",
    "text",
    "block-insert"
)
foreach ($name in $runtimeToolIcons) {
    if (-not ($manifest.name -contains $name)) {
        Fail ("Runtime tool has no SuicaCad icon asset: " + $name)
    }
}

$source = Get-Content -Raw -Path $mainWinPath
$renderer = [regex]::Match(
    $source,
    'void\s+draw_tool_icon\s*\(.*?\n\}\s*\n\s*void\s+draw_toolbar',
    [System.Text.RegularExpressions.RegexOptions]::Singleline)
if (!$renderer.Success) {
    Fail "Could not isolate draw_tool_icon renderer."
}
$toolCases = [regex]::Matches(
    $renderer.Value,
    'case\s+Tool::([A-Za-z0-9_]+)\s*:') |
    ForEach-Object { $_.Groups[1].Value } |
    Select-Object -Unique

$requiredToolCases = @(
    "Select", "Line", "Polyline", "Rectangle", "Circle", "Arc",
    "Move", "Copy", "Rotate", "Scale", "Mirror", "Trim", "Extend",
    "Offset", "Dimension", "Hatch", "Text", "BlockInsert"
)
foreach ($tool in $requiredToolCases) {
    if ($toolCases -notcontains $tool) {
        Fail ("draw_tool_icon is missing runtime tool renderer: " + $tool)
    }
}

foreach ($name in @("visible", "lock")) {
    if (-not ($manifest.name -contains $name)) {
        Fail ("Layer panel icon asset missing: " + $name)
    }
}

Write-Host ("SuicaCad icon asset contract passed: {0} icons, {1} runtime tool renderers." -f
    $manifest.Count, $requiredToolCases.Count)
