$ErrorActionPreference = "Stop"

param(
    [Parameter(Mandatory = $true)]
    [string]$Exe
)

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class GuiTestNative {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
}
"@

function Send-Key([IntPtr]$hwnd, [int]$vk) {
    [GuiTestNative]::SendMessage($hwnd, 0x0100, [IntPtr]$vk, [IntPtr]::Zero) | Out-Null
}

function Click-Client([IntPtr]$hwnd, [int]$x, [int]$y) {
    $packed = (($y -band 0xFFFF) -shl 16) -bor ($x -band 0xFFFF)
    [GuiTestNative]::SendMessage($hwnd, 0x0201, [IntPtr]1, [IntPtr]$packed) | Out-Null
}

function Write-Snapshot([IntPtr]$hwnd, [string]$path) {
    $env:ACP_GUI_TEST_SNAPSHOT = $path
    $result = [GuiTestNative]::SendMessage($hwnd, 0x8000 + 42, [IntPtr]::Zero, [IntPtr]::Zero)
    if ($result.ToInt64() -ne 1) {
        throw "GUI snapshot hook failed for $path"
    }
    if (!(Test-Path $path)) {
        throw "GUI snapshot was not created: $path"
    }
}

New-Item -ItemType Directory -Force -Path artifacts | Out-Null
$beforeDelete = Join-Path $PWD "artifacts\gui-interaction-before-delete.acp2d"
$afterDelete = Join-Path $PWD "artifacts\gui-interaction-after-delete.acp2d"
Remove-Item $beforeDelete, $afterDelete -ErrorAction SilentlyContinue

$proc = Start-Process -FilePath $Exe -PassThru
try {
    $deadline = (Get-Date).AddSeconds(20)
    do {
        Start-Sleep -Milliseconds 300
        $proc.Refresh()
    } while ($proc.MainWindowHandle -eq 0 -and !$proc.HasExited -and (Get-Date) -lt $deadline)

    if ($proc.HasExited) {
        throw "AutoCADPro exited before GUI interaction test. ExitCode=$($proc.ExitCode)"
    }
    if ($proc.MainWindowHandle -eq 0) {
        throw "AutoCADPro did not create a main window for GUI interaction test"
    }

    $hwnd = $proc.MainWindowHandle
    [GuiTestNative]::SetForegroundWindow($hwnd) | Out-Null

    $rect = New-Object GuiTestNative+RECT
    if (![GuiTestNative]::GetClientRect($hwnd, [ref]$rect)) {
        throw "GetClientRect failed"
    }

    if (($rect.Right - $rect.Left) -lt 900 -or ($rect.Bottom - $rect.Top) -lt 600) {
        throw "Unexpected client area for deterministic interaction test"
    }

    # LINE: real keyboard tool selection + two real window click messages.
    Send-Key $hwnd 0x4C # L
    Click-Client $hwnd 300 260
    Click-Client $hwnd 520 260

    # CIRCLE.
    Send-Key $hwnd 0x43 # C
    Click-Client $hwnd 650 360
    Click-Client $hwnd 710 360

    # RECTANGLE, persisted as a closed polyline.
    Send-Key $hwnd 0x42 # B
    Click-Client $hwnd 360 430
    Click-Client $hwnd 520 540

    Write-Snapshot $hwnd $beforeDelete
    $before = Get-Content -Raw -Path $beforeDelete

    if ($before -notmatch '(?m)^E\s+\d+\s+\d+\s+\d+\s+1\s+0\s+0\.25\s+LINE\s+') {
        throw "GUI Line interaction did not persist a LINE entity"
    }
    if ($before -notmatch '(?m)^E\s+\d+\s+\d+\s+\d+\s+1\s+0\s+0\.25\s+CIRCLE\s+') {
        throw "GUI Circle interaction did not persist a CIRCLE entity"
    }
    if ($before -notmatch '(?m)^E\s+\d+\s+\d+\s+\d+\s+1\s+0\s+0\.25\s+POLYLINE\s+') {
        throw "GUI Rectangle interaction did not persist a closed POLY entity"
    }

    # Escape returns to Select. Select the known line midpoint and Delete it.
    Send-Key $hwnd 0x1B
    Click-Client $hwnd 410 260
    Send-Key $hwnd 0x2E

    Write-Snapshot $hwnd $afterDelete
    $after = Get-Content -Raw -Path $afterDelete

    if ($after -match '(?m)^E\s+\d+\s+\d+\s+\d+\s+1\s+0\s+0\.25\s+LINE\s+') {
        throw "GUI Select/Delete interaction failed to remove the line"
    }
    if ($after -notmatch '(?m)^E\s+\d+\s+\d+\s+\d+\s+1\s+0\s+0\.25\s+CIRCLE\s+') {
        throw "Delete regression: Circle was lost while deleting Line"
    }
    if ($after -notmatch '(?m)^E\s+\d+\s+\d+\s+\d+\s+1\s+0\s+0\.25\s+POLYLINE\s+') {
        throw "Delete regression: Rectangle was lost while deleting Line"
    }

    Write-Host "GUI interaction regression test passed"
}
finally {
    if (!$proc.HasExited) {
        Stop-Process -Id $proc.Id -Force
    }
    Remove-Item Env:ACP_GUI_TEST_SNAPSHOT -ErrorAction SilentlyContinue
}
