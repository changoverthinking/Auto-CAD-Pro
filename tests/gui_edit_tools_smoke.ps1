param(
    [Parameter(Mandatory = $true)]
    [string]$Exe
)

$ErrorActionPreference = "Stop"

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class GuiEditNative {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
}
"@

function Send-Key([IntPtr]$hwnd, [int]$vk) {
    [GuiEditNative]::SendMessage($hwnd, 0x0100, [IntPtr]$vk, [IntPtr]::Zero) | Out-Null
}

function Send-CtrlKey([IntPtr]$hwnd, [int]$vk) {
    [GuiEditNative]::SetForegroundWindow($hwnd) | Out-Null
    [GuiEditNative]::keybd_event(0x11, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 50
    [GuiEditNative]::SendMessage($hwnd, 0x0100, [IntPtr]$vk, [IntPtr]::Zero) | Out-Null
    [GuiEditNative]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero)
}

function Click-Client([IntPtr]$hwnd, [int]$x, [int]$y) {
    $packed = (($y -band 0xFFFF) -shl 16) -bor ($x -band 0xFFFF)
    [GuiEditNative]::SendMessage($hwnd, 0x0201, [IntPtr]1, [IntPtr]$packed) | Out-Null
}

function Snapshot([IntPtr]$hwnd, [int]$id) {
    $path = Join-Path $PWD ("artifacts\gui-edit-tools-" + $id + ".acp2d")
    Remove-Item $path -ErrorAction SilentlyContinue
    $result = [GuiEditNative]::SendMessage($hwnd, 0x8000 + 42, [IntPtr]$id, [IntPtr]::Zero)
    if ($result.ToInt64() -ne 1 -or !(Test-Path $path)) {
        throw "Could not create GUI edit snapshot $id"
    }
    return Get-Content -Raw -Path $path
}

function Line-Count([string]$data) {
    return ($data | Select-String -Pattern '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+0(?:\.0+)?\s+LINE\s+' -AllMatches).Matches.Count
}

New-Item -ItemType Directory -Force -Path artifacts | Out-Null
Get-ChildItem artifacts -Filter "gui-edit-tools-*.acp2d" -ErrorAction SilentlyContinue |
    Remove-Item -Force
$env:ACP_GUI_TEST_SNAPSHOT_DIR = (Join-Path $PWD "artifacts")

$proc = Start-Process -FilePath $Exe -PassThru
try {
    $deadline = (Get-Date).AddSeconds(20)
    do {
        Start-Sleep -Milliseconds 300
        $proc.Refresh()
    } while ($proc.MainWindowHandle -eq 0 -and !$proc.HasExited -and (Get-Date) -lt $deadline)

    if ($proc.HasExited -or $proc.MainWindowHandle -eq 0) {
        throw "AutoCADPro did not create a window for edit-tools regression"
    }

    $hwnd = $proc.MainWindowHandle
    [GuiEditNative]::SetForegroundWindow($hwnd) | Out-Null

    $rect = New-Object GuiEditNative+RECT
    if (![GuiEditNative]::GetClientRect($hwnd, [ref]$rect) -or
        ($rect.Right - $rect.Left) -lt 900 -or
        ($rect.Bottom - $rect.Top) -lt 600) {
        throw "Unexpected client area for edit-tools regression"
    }

    # Disable Object Snap for deterministic raw input.
    Send-Key $hwnd 0x72 # F3

    # Base line.
    Send-Key $hwnd 0x4C # L
    Click-Client $hwnd 300 250
    Click-Client $hwnd 450 250
    $base = Snapshot $hwnd 20
    if ((Line-Count $base) -ne 1) {
        throw "Edit regression setup did not create exactly one base line"
    }

    # MOVE.
    Send-Key $hwnd 0x4D # M
    Click-Client $hwnd 300 250
    Click-Client $hwnd 320 270
    $moved = Snapshot $hwnd 21
    if ($moved -eq $base) {
        throw "GUI Move did not modify document geometry"
    }

    # Real GUI Undo/Redo around Move.
    Send-CtrlKey $hwnd 0x5A # Ctrl+Z
    $moveUndone = Snapshot $hwnd 22
    if ($moveUndone -ne $base) {
        throw "GUI Move Undo did not restore the original document"
    }
    Send-CtrlKey $hwnd 0x59 # Ctrl+Y
    $moveRedone = Snapshot $hwnd 23
    if ($moveRedone -ne $moved) {
        throw "GUI Move Redo did not restore moved geometry"
    }

    # COPY.
    Send-Key $hwnd 0x59 # Y
    Click-Client $hwnd 320 270
    Click-Client $hwnd 360 310
    $copied = Snapshot $hwnd 24
    if ((Line-Count $copied) -ne 2) {
        throw "GUI Copy did not add an independent line"
    }

    # ROTATE selected copy.
    Send-Key $hwnd 0x52 # R
    Click-Client $hwnd 360 310
    Click-Client $hwnd 360 250
    $rotated = Snapshot $hwnd 25
    if ($rotated -eq $copied) {
        throw "GUI Rotate did not modify selected line"
    }

    # SCALE selected copy: base, reference, target.
    Send-Key $hwnd 0x53 # S
    Click-Client $hwnd 360 310
    Click-Client $hwnd 400 310
    Click-Client $hwnd 440 310
    $scaled = Snapshot $hwnd 26
    if ($scaled -eq $rotated) {
        throw "GUI Scale did not modify selected line"
    }

    # MIRROR selected copy around a vertical axis.
    Send-Key $hwnd 0x49 # I
    Click-Client $hwnd 500 220
    Click-Client $hwnd 500 420
    $mirrored = Snapshot $hwnd 27
    if ($mirrored -eq $scaled) {
        throw "GUI Mirror did not modify selected line"
    }

    # OFFSET selected line.
    Send-Key $hwnd 0x4F # O
    Click-Client $hwnd 430 360
    $offset = Snapshot $hwnd 28
    if ((Line-Count $offset) -ne 3) {
        throw "GUI Offset did not create a third line"
    }

    # TRIM setup: target horizontal + cutter vertical.
    Send-Key $hwnd 0x4C
    Click-Client $hwnd 600 250
    Click-Client $hwnd 800 250
    Send-Key $hwnd 0x4C
    Click-Client $hwnd 700 180
    Click-Client $hwnd 700 320

    Send-Key $hwnd 0x1B
    Click-Client $hwnd 650 250
    Send-Key $hwnd 0x54 # T
    $trimBefore = Snapshot $hwnd 29
    Click-Client $hwnd 700 220
    Click-Client $hwnd 620 250
    $trimAfter = Snapshot $hwnd 30
    if ($trimAfter -eq $trimBefore) {
        throw "GUI Trim did not modify target geometry"
    }

    # EXTEND setup: short target + vertical boundary.
    Send-Key $hwnd 0x4C
    Click-Client $hwnd 600 400
    Click-Client $hwnd 650 400
    Send-Key $hwnd 0x4C
    Click-Client $hwnd 750 350
    Click-Client $hwnd 750 450

    Send-Key $hwnd 0x1B
    Click-Client $hwnd 625 400
    Send-Key $hwnd 0x45 # E
    $extendBefore = Snapshot $hwnd 31
    Click-Client $hwnd 750 400
    $extendAfter = Snapshot $hwnd 32
    if ($extendAfter -eq $extendBefore) {
        throw "GUI Extend did not modify target geometry"
    }

    Write-Host "GUI edit-tools regression test passed"
}
finally {
    if (!$proc.HasExited) {
        Stop-Process -Id $proc.Id -Force
    }
    Remove-Item Env:ACP_GUI_TEST_SNAPSHOT_DIR -ErrorAction SilentlyContinue
}
