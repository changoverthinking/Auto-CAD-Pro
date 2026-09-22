param(
    [Parameter(Mandatory = $true)]
    [string]$Exe
)

$ErrorActionPreference = "Stop"

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class GuiVisibilityNative {
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessage(
        IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
}
"@

function Pack-Point([int]$x, [int]$y) {
    return [IntPtr]((($y -band 0xFFFF) -shl 16) -bor ($x -band 0xFFFF))
}

function Click-Client([IntPtr]$hwnd, [int]$x, [int]$y) {
    [GuiVisibilityNative]::SendMessage(
        $hwnd, 0x0201, [IntPtr]1, (Pack-Point $x $y)) | Out-Null
}

function Send-Key([IntPtr]$hwnd, [int]$vk) {
    [GuiVisibilityNative]::SendMessage(
        $hwnd, 0x0100, [IntPtr]$vk, [IntPtr]::Zero) | Out-Null
}

function Snapshot([IntPtr]$hwnd, [int]$id) {
    [GuiVisibilityNative]::SendMessage(
        $hwnd, 0x8000 + 42, [IntPtr]$id, [IntPtr]::Zero) | Out-Null
    $path = Join-Path $PWD ("artifacts\gui-interaction-" + $id + ".acp2d")
    if (!(Test-Path $path)) {
        throw "Entity visibility regression: snapshot $id missing"
    }
    return Get-Content -Raw -Path $path
}

New-Item -ItemType Directory -Force -Path artifacts | Out-Null
$env:ACP_GUI_TEST_SNAPSHOT_DIR = (Join-Path $PWD "artifacts")
foreach ($id in 92, 93, 94) {
    Remove-Item (Join-Path $PWD ("artifacts\gui-interaction-" + $id + ".acp2d")) -Force -ErrorAction SilentlyContinue
}

$proc = Start-Process -FilePath $Exe -PassThru
try {
    $deadline = (Get-Date).AddSeconds(20)
    do {
        Start-Sleep -Milliseconds 300
        $proc.Refresh()
    } while ($proc.MainWindowHandle -eq 0 -and !$proc.HasExited -and (Get-Date) -lt $deadline)

    if ($proc.HasExited -or $proc.MainWindowHandle -eq 0) {
        throw "Entity visibility regression: application did not create a main window"
    }

    $hwnd = $proc.MainWindowHandle
    [GuiVisibilityNative]::SetForegroundWindow($hwnd) | Out-Null

    Send-Key $hwnd 0x4C
    Click-Client $hwnd 260 220
    Click-Client $hwnd 420 220

    $visible = Snapshot $hwnd 92
    if ($visible -notmatch '(?m)^E\s+\d+\s+1\s+1\s+0\s+0(?:\.0+)?\s+LINE\s+') {
        throw "Entity visibility regression: initial line is not visible"
    }

    [GuiVisibilityNative]::SendMessage(
        $hwnd, 0x0111, [IntPtr]1012, [IntPtr]::Zero) | Out-Null
    $hidden = Snapshot $hwnd 93
    if ($hidden -notmatch '(?m)^E\s+\d+\s+1\s+0\s+0\s+0(?:\.0+)?\s+LINE\s+') {
        throw "Entity visibility regression: first toggle did not hide the line"
    }

    [GuiVisibilityNative]::SendMessage(
        $hwnd, 0x0111, [IntPtr]1012, [IntPtr]::Zero) | Out-Null
    $restored = Snapshot $hwnd 94
    if ($restored -notmatch '(?m)^E\s+\d+\s+1\s+1\s+0\s+0(?:\.0+)?\s+LINE\s+') {
        throw "Entity visibility regression: second toggle could not restore the hidden line"
    }

    Write-Host "GUI entity visibility regression test passed"
}
finally {
    if (!$proc.HasExited) {
        Stop-Process -Id $proc.Id -Force
    }
    Remove-Item Env:ACP_GUI_TEST_SNAPSHOT_DIR -ErrorAction SilentlyContinue
}
