param(
    [Parameter(Mandatory = $true)]
    [string]$Exe
)

$ErrorActionPreference = "Stop"

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class GuiQuickToolbarNative {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessage(
        IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
}
"@

function Pack-Point([int]$x, [int]$y) {
    return [IntPtr]((($y -band 0xFFFF) -shl 16) -bor ($x -band 0xFFFF))
}

function Click-Client([IntPtr]$hwnd, [int]$x, [int]$y) {
    [GuiQuickToolbarNative]::SendMessage(
        $hwnd, 0x0201, [IntPtr]1, (Pack-Point $x $y)) | Out-Null
}

function Send-Key([IntPtr]$hwnd, [int]$vk) {
    [GuiQuickToolbarNative]::SendMessage(
        $hwnd, 0x0100, [IntPtr]$vk, [IntPtr]::Zero) | Out-Null
}

function Snapshot([IntPtr]$hwnd, [int]$id) {
    $path = Join-Path $PWD ("artifacts\gui-interaction-" + $id + ".acp2d")
    Remove-Item $path -Force -ErrorAction SilentlyContinue
    $result = [GuiQuickToolbarNative]::SendMessage(
        $hwnd, 0x8000 + 42, [IntPtr]$id, [IntPtr]::Zero)
    if ($result.ToInt64() -ne 1 -or !(Test-Path $path)) {
        throw "Quick toolbar regression: snapshot $id missing"
    }
    return Get-Content -Raw -Path $path
}

New-Item -ItemType Directory -Force -Path artifacts | Out-Null
$env:ACP_GUI_TEST_SNAPSHOT_DIR = (Join-Path $PWD "artifacts")

$proc = Start-Process -FilePath $Exe -PassThru
try {
    $deadline = (Get-Date).AddSeconds(20)
    do {
        Start-Sleep -Milliseconds 300
        $proc.Refresh()
    } while ($proc.MainWindowHandle -eq 0 -and !$proc.HasExited -and (Get-Date) -lt $deadline)

    if ($proc.HasExited -or $proc.MainWindowHandle -eq 0) {
        throw "Quick toolbar regression: application did not create a main window"
    }

    $hwnd = $proc.MainWindowHandle
    [GuiQuickToolbarNative]::SetForegroundWindow($hwnd) | Out-Null

    $rect = New-Object GuiQuickToolbarNative+RECT
    if (![GuiQuickToolbarNative]::GetClientRect($hwnd, [ref]$rect)) {
        throw "Quick toolbar regression: GetClientRect failed"
    }

    $clientHeight = $rect.Bottom - $rect.Top
    $toolbarHeight = [Math]::Min(
        34, [Math]::Max(28, [Math]::Floor($clientHeight / 32)))
    $buttonSize = [Math]::Min(
        28, [Math]::Max(22, $toolbarHeight - 6))
    $gap = 1
    $buttonTop = [Math]::Max(
        1, [Math]::Floor(($toolbarHeight - $buttonSize) / 2))

    # Create one line with the normal tool path.
    Send-Key $hwnd 0x4C
    Click-Client $hwnd 260 220
    Click-Client $hwnd 420 220

    $created = Snapshot $hwnd 95
    if ($created -notmatch '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+[^\s]+\s+LINE\s+') {
        throw "Quick toolbar regression: line was not created"
    }

    # Quick toolbar order: New, Open, Save, Undo, Redo, Fit.
    $undoIndex = 3
    $redoIndex = 4
    $undoX = 3 + ($undoIndex * ($buttonSize + $gap)) + [Math]::Floor($buttonSize / 2)
    $redoX = 3 + ($redoIndex * ($buttonSize + $gap)) + [Math]::Floor($buttonSize / 2)
    $buttonY = $buttonTop + [Math]::Floor($buttonSize / 2)

    Click-Client $hwnd $undoX $buttonY
    $undone = Snapshot $hwnd 96
    if ($undone -match '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+[^\s]+\s+LINE\s+') {
        throw "Quick toolbar regression: Undo icon did not remove the line"
    }

    Click-Client $hwnd $redoX $buttonY
    $redone = Snapshot $hwnd 97
    if ($redone -notmatch '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+[^\s]+\s+LINE\s+') {
        throw "Quick toolbar regression: Redo icon did not restore the line"
    }

    Write-Host "GUI quick toolbar regression test passed"
}
finally {
    if (!$proc.HasExited) {
        Stop-Process -Id $proc.Id -Force
    }
    Remove-Item Env:ACP_GUI_TEST_SNAPSHOT_DIR -ErrorAction SilentlyContinue
}
