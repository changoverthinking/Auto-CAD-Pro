param()

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot

function Fail([string]$message) {
    Write-Error $message
    exit 1
}

# 1) Reject unresolved merge markers in source, tests, workflows and build scripts.
$scanRoots = @(
    (Join-Path $repoRoot "src"),
    (Join-Path $repoRoot "include"),
    (Join-Path $repoRoot "tests"),
    (Join-Path $repoRoot ".github"),
    (Join-Path $repoRoot "installer")
)

$scanFiles = foreach ($root in $scanRoots) {
    if (Test-Path $root) {
        Get-ChildItem -Path $root -Recurse -File -ErrorAction Stop
    }
}

$scanFiles += Get-Item (Join-Path $repoRoot "CMakeLists.txt")

$markerPattern = '^(<<<<<<<|=======|>>>>>>>)'
foreach ($file in $scanFiles) {
    $matches = Select-String -Path $file.FullName -Pattern $markerPattern -ErrorAction SilentlyContinue
    if ($matches) {
        Fail ("Unresolved merge/conflict marker detected in " + $file.FullName)
    }
}

# 2) Parse every PowerShell script before any expensive configure/build step.
$parseFailures = New-Object System.Collections.Generic.List[string]
Get-ChildItem -Path $repoRoot -Recurse -Filter *.ps1 -File | ForEach-Object {
    $tokens = $null
    $errors = $null
    [System.Management.Automation.Language.Parser]::ParseFile(
        $_.FullName,
        [ref]$tokens,
        [ref]$errors
    ) | Out-Null

    if ($errors.Count -gt 0) {
        foreach ($error in $errors) {
            $parseFailures.Add(
                ("{0}:{1}:{2} {3}" -f
                    $_.FullName,
                    $error.Extent.StartLineNumber,
                    $error.Extent.StartColumnNumber,
                    $error.Message)
            )
        }
    }
}

if ($parseFailures.Count -gt 0) {
    $parseFailures | ForEach-Object { Write-Error $_ }
    Fail "PowerShell syntax validation failed."
}

# 3) Guard historically fragile GUI regression scripts from accidental bulk duplication.
# This is deliberately generous: it detects catastrophic expansion, not normal test growth.
$guardedScripts = @{
    "tests\gui_property_editor_smoke.ps1" = 1200
    "tests\gui_interaction_smoke.ps1"     = 1600
    "tests\gui_edit_tools_smoke.ps1"      = 1600
    "tests\gui_layout_dirty_smoke.ps1"    = 1000
}

foreach ($entry in $guardedScripts.GetEnumerator()) {
    $path = Join-Path $repoRoot $entry.Key
    if (!(Test-Path $path)) {
        Fail ("Expected guarded regression script is missing: " + $entry.Key)
    }

    $lineCount = (Get-Content -Path $path).Count
    if ($lineCount -gt $entry.Value) {
        Fail (
            "Regression script grew beyond safety threshold: {0} has {1} lines (limit {2}). " +
            "Review for accidental duplication/corruption before raising the threshold." -f
                $entry.Key, $lineCount, $entry.Value
        )
    }
}

# 4) Detect exact repeated large blocks inside the historically fragile property script.
# A repeated 40-line block strongly indicates accidental paste/append corruption.
$propertyScript = Join-Path $repoRoot "tests\gui_property_editor_smoke.ps1"
$lines = Get-Content -Path $propertyScript
$blockSize = 40
$seen = @{}
for ($i = 0; $i -le $lines.Count - $blockSize; $i += $blockSize) {
    $block = ($lines[$i..($i + $blockSize - 1)] -join "`n").Trim()
    if ([string]::IsNullOrWhiteSpace($block)) {
        continue
    }

    $bytes = [System.Text.Encoding]::UTF8.GetBytes($block)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $hash = [Convert]::ToHexString($sha.ComputeHash($bytes))
    }
    finally {
        $sha.Dispose()
    }

    if ($seen.ContainsKey($hash)) {
        Fail (
            "Exact repeated 40-line block detected in tests\gui_property_editor_smoke.ps1 " +
            "(blocks starting near lines {0} and {1}). Review for accidental duplication." -f
                ($seen[$hash] + 1), ($i + 1)
        )
    }
    $seen[$hash] = $i
}

Write-Host "Source sanity checks passed."
