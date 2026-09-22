param()

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$cmake = Get-Content -Raw (Join-Path $repoRoot "CMakeLists.txt")
$workflow = Get-Content -Raw (Join-Path $repoRoot ".github\workflows\windows-ci.yml")

function Require([string]$text, [string]$needle, [string]$message) {
    if ($text -notmatch [regex]::Escape($needle)) {
        Write-Error $message
        exit 1
    }
}

$requiredCTestTargets = @(
    "acp_core_tests",
    "acp_workflow_regression_tests",
    "acp_annotation_properties_workflow_tests",
    "acp_property_edit_tests",
    "acp_color_linetype_tests",
    "acp_recovery_tests",
    "acp_layout_print_workflow_tests",
    "acp_large_document_tests",
    "acp_ui_layout_tests"
)
foreach ($target in $requiredCTestTargets) {
    Require $cmake $target "Release contract lost required CTest target: $target"
}

$requiredWorkflowSteps = @(
    "Source sanity preflight",
    "GUI command reachability preflight",
    "SuicaCad icon asset preflight",
    "Configure",
    "Build",
    "Test",
    "Repeat core regression suite",
    "Launch GUI and capture real window",
    "Repeated GUI lifecycle smoke",
    "GUI interaction regression",
    "GUI edit-tools regression",
    "GUI layout dirty-state regression",
    "GUI responsive panel regression",
    "GUI entity visibility regression",
    "GUI direct property editor regression",
    "GUI Color and Linetype regression",
    "Build versioned portable package",
    "Validate NSIS installer build",
    "Stage standalone Windows application"
)
foreach ($step in $requiredWorkflowSteps) {
    Require $workflow $step "Release contract lost required CI gate: $step"
}

$requiredFiles = @(
    "tests\ci_icon_assets.ps1",
    "assets\icons\suicacad\manifest.tsv",
    "assets\icons\suicacad\suicacad-icons-extended.svg",
    "tests\gui_interaction_smoke.ps1",
    "tests\gui_edit_tools_smoke.ps1",
    "tests\gui_layout_dirty_smoke.ps1",
    "tests\gui_responsive_panel_smoke.ps1",
    "tests\gui_entity_visibility_smoke.ps1",
    "tests\gui_property_editor_smoke.ps1",
    "tests\gui_color_linetype_smoke.ps1",
    "tests\large_document_tests.cpp",
    "tests\recovery_tests.cpp",
    "installer\AutoCADPro.nsi",
    ".github\workflows\release.yml"
)
foreach ($relative in $requiredFiles) {
    if (!(Test-Path (Join-Path $repoRoot $relative))) {
        Write-Error "Release contract missing required file: $relative"
        exit 1
    }
}

Write-Host "2D release contract passed."
