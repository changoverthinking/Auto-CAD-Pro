param(
    [Parameter(Mandatory = $true)]
    [string]$Exe
)

$ErrorActionPreference = "Stop"

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class GuiGroupNative {
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
    [GuiGroupNative]::SendMessage($hwnd, 0x0100, [IntPtr]$vk, [IntPtr]::Zero) | Out-Null
}

function Send-CtrlKey([IntPtr]$hwnd, [int]$vk) {
    [GuiGroupNative]::SetForegroundWindow($hwnd) | Out-Null
    [GuiGroupNative]::keybd_event(0x11, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 50
    [GuiGroupNative]::SendMessage($hwnd, 0x0100, [IntPtr]$vk, [IntPtr]::Zero) | Out-Null
    [GuiGroupNative]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero)
}

function Click-Client([IntPtr]$hwnd, [int]$x, [int]$y) {
    $packed = (($y -band 0xFFFF) -shl 16) -bor ($x -band 0xFFFF)
    [GuiGroupNative]::SendMessage($hwnd, 0x0201, [IntPtr]1, [IntPtr]$packed) | Out-Null
}

function Snapshot([IntPtr]$hwnd, [int]$id) {
    $path = Join-Path $PWD ("artifacts\gui-interaction-" + $id + ".acp2d")
    Remove-Item $path -ErrorAction SilentlyContinue
    $result = [GuiGroupNative]::SendMessage($hwnd, 0x8000 + 42, [IntPtr]$id, [IntPtr]::Zero)
    if ($result.ToInt64() -ne 1 -or !(Test-Path $path)) {
        throw "Could not create GUI edit snapshot $id"
    }
    return Get-Content -Raw -Path $path
}

function Line-Count([string]$data) {
    return ($data | Select-String -Pattern '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+0(?:\.0+)?\s+LINE\s+' -AllMatches).Matches.Count
}

New-Item -ItemType Directory -Force -Path artifacts | Out-Null
Get-ChildItem artifacts -Filter "gui-interaction-8*.acp2d" -ErrorAction SilentlyContinue |
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
        throw "AutoCADPro did not create a window for group-selection regression"
    }

    $hwnd = $proc.MainWindowHandle
    [GuiGroupNative]::SetForegroundWindow($hwnd) | Out-Null

    $rect = New-Object GuiGroupNative+RECT
    if (![GuiGroupNative]::GetClientRect($hwnd, [ref]$rect) -or
        ($rect.Right - $rect.Left) -lt 900 -or
        ($rect.Bottom - $rect.Top) -lt 600) {
        throw "Unexpected client area for group-selection regression"
    }

    # Disable Object Snap for deterministic raw input.
    Send-Key $hwnd 0x72 # F3

    # Three separate members allow us to distinguish multi-edit from primary-only edit.
    foreach ($y in @(250, 300, 350)) {
        Send-Key $hwnd 0x4C
        Click-Client $hwnd 220 $y
        Click-Client $hwnd 360 $y
    }
    $base = Snapshot $hwnd 80
    if ((Line-Count $base) -ne 3) { throw "Could not create group fixture" }

    Send-Key $hwnd 0x1B # Select
    Click-Client $hwnd 270 250
    [GuiGroupNative]::keybd_event(0x11, 0, 0, [UIntPtr]::Zero)
    try {
        Start-Sleep -Milliseconds 50
        Click-Client $hwnd 270 300
        Click-Client $hwnd 270 350
        Click-Client $hwnd 270 300 # Toggle middle member off.
    } finally {
        [GuiGroupNative]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero)
    }
    Send-Key $hwnd 0x2E # Delete first and third, leaving second.
    $subset = Snapshot $hwnd 81
    if ((Line-Count $subset) -ne 1 -or $subset -notmatch '(?m)^E 2 ') {
        throw "Ctrl-click did not toggle only the intended members"
    }
    Send-CtrlKey $hwnd 0x5A
    if ((Snapshot $hwnd 82) -ne $base) { throw "Group Delete undo did not restore all members" }
    Send-CtrlKey $hwnd 0x59
    if ((Snapshot $hwnd 83) -ne $subset) { throw "Group Delete redo mismatch" }
    Send-CtrlKey $hwnd 0x5A

    # Ctrl+A must select all editable members without switching to Arc.
    Send-CtrlKey $hwnd 0x41
    foreach ($tool in @(0x4D, 0x52, 0x53, 0x49)) { # Move, Rotate, Scale, Mirror
        Send-Key $hwnd $tool
        Click-Client $hwnd 260 400
        if ($tool -eq 0x52) { Click-Client $hwnd 260 450 }
        else { Click-Client $hwnd 320 400 }
        if ($tool -eq 0x53) { Click-Client $hwnd 380 400 }
        $changed = Snapshot $hwnd 84
        if ((Line-Count $changed) -ne 3) { throw "Transform changed group entity count" }
        $oldRows = ($base -split "`n") | Where-Object { $_ -match '^E ' }
        $newRows = ($changed -split "`n") | Where-Object { $_ -match '^E ' }
        for ($i = 0; $i -lt 3; $i++) {
            if ($oldRows[$i] -eq $newRows[$i]) { throw "Tool $tool failed to transform member $i" }
        }
        Send-CtrlKey $hwnd 0x5A
        if ((Snapshot $hwnd 85) -ne $base) { throw "Tool $tool group undo mismatch" }
        Send-CtrlKey $hwnd 0x59
        if ((Snapshot $hwnd 86) -ne $changed) { throw "Tool $tool group redo mismatch" }
        Send-CtrlKey $hwnd 0x5A
    }

    # Menu route also reaches Select All.
    [GuiGroupNative]::SendMessage($hwnd, 0x0111, [IntPtr]1055, [IntPtr]::Zero) | Out-Null
    Send-Key $hwnd 0x59 # Copy
    Click-Client $hwnd 260 400
    Click-Client $hwnd 360 400
    $copied = Snapshot $hwnd 87
    if ((Line-Count $copied) -ne 6) { throw "Copy did not duplicate all members" }
    Send-CtrlKey $hwnd 0x5A
    if ((Snapshot $hwnd 88) -ne $base) { throw "Copy group undo mismatch" }
    Send-CtrlKey $hwnd 0x59
    if ((Snapshot $hwnd 89) -ne $copied) { throw "Copy group redo mismatch" }

    Send-CtrlKey $hwnd 0x41
    Send-Key $hwnd 0x2E
    if ((Line-Count (Snapshot $hwnd 90)) -ne 0) { throw "Select All/Delete left members behind" }
    Send-CtrlKey $hwnd 0x5A
    if ((Snapshot $hwnd 91) -ne $copied) { throw "Delete six members did not undo in one step" }
    Write-Host "GUI group-selection regression passed"
}
finally {
    if (!$proc.HasExited) { Stop-Process -Id $proc.Id -Force }
    Remove-Item Env:ACP_GUI_TEST_SNAPSHOT_DIR -ErrorAction SilentlyContinue
}
