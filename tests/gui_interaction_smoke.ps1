param(
    [Parameter(Mandatory = $true)]
    [string]$Exe
)

$ErrorActionPreference = "Stop"

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

    [DllImport("user32.dll")]
    public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetClassName(IntPtr hWnd, System.Text.StringBuilder lpClassName, int nMaxCount);

    public static IntPtr FindDialogForProcess(uint processId) {
        IntPtr found = IntPtr.Zero;
        EnumWindows(delegate(IntPtr hWnd, IntPtr lParam) {
            uint pid;
            GetWindowThreadProcessId(hWnd, out pid);
            if (pid != processId) return true;

            var className = new System.Text.StringBuilder(64);
            GetClassName(hWnd, className, className.Capacity);
            if (className.ToString() == "#32770") {
                found = hWnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
"@

function Send-Key([IntPtr]$hwnd, [int]$vk) {
    [GuiTestNative]::SendMessage($hwnd, 0x0100, [IntPtr]$vk, [IntPtr]::Zero) | Out-Null
}

function Send-CtrlKey([IntPtr]$hwnd, [int]$vk) {
    [GuiTestNative]::SetForegroundWindow($hwnd) | Out-Null
    [GuiTestNative]::keybd_event(0x11, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 50
    [GuiTestNative]::SendMessage($hwnd, 0x0100, [IntPtr]$vk, [IntPtr]::Zero) | Out-Null
    [GuiTestNative]::keybd_event(0x11, 0, 2, [UIntPtr]::Zero)
}

function Send-Char([IntPtr]$hwnd, [int]$charCode) {
    [GuiTestNative]::SendMessage($hwnd, 0x0102, [IntPtr]$charCode, [IntPtr]::Zero) | Out-Null
}

function Click-Client([IntPtr]$hwnd, [int]$x, [int]$y) {
    $packed = (($y -band 0xFFFF) -shl 16) -bor ($x -band 0xFFFF)
    [GuiTestNative]::SendMessage($hwnd, 0x0201, [IntPtr]1, [IntPtr]$packed) | Out-Null
}

function Write-Snapshot([IntPtr]$hwnd, [int]$id, [string]$path) {
    $result = [GuiTestNative]::SendMessage($hwnd, 0x8000 + 42, [IntPtr]$id, [IntPtr]::Zero)
    if ($result.ToInt64() -ne 1) {
        throw "GUI snapshot hook failed for $path"
    }
    if (!(Test-Path $path)) {
        throw "GUI snapshot was not created: $path"
    }
}

New-Item -ItemType Directory -Force -Path artifacts | Out-Null
$beforeDelete = Join-Path $PWD "artifacts\gui-interaction-1.acp2d"
$afterDelete = Join-Path $PWD "artifacts\gui-interaction-2.acp2d"
$layerCreated = Join-Path $PWD "artifacts\gui-interaction-3.acp2d"
$layerUndone = Join-Path $PWD "artifacts\gui-interaction-4.acp2d"
$afterUndoDraw = Join-Path $PWD "artifacts\gui-interaction-5.acp2d"
Remove-Item $beforeDelete, $afterDelete, $layerCreated, $layerUndone, $afterUndoDraw -ErrorAction SilentlyContinue
$env:ACP_GUI_TEST_SNAPSHOT_DIR = (Join-Path $PWD "artifacts")
$recoveryPath = Join-Path $env:LOCALAPPDATA "AutoCADPro\recovery.acp"
Remove-Item $recoveryPath -ErrorAction SilentlyContinue

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

    # HATCH the selected rectangle.
    Send-Key $hwnd 0x48 # H
    Click-Client $hwnd 440 485

    # ARC: center, radius point, end-angle point.
    Send-Key $hwnd 0x41 # A
    Click-Client $hwnd 650 470
    Click-Client $hwnd 700 470
    Click-Client $hwnd 650 420

    # POLYLINE completed with Enter.
    Send-Key $hwnd 0x50 # P
    Click-Client $hwnd 300 600
    Click-Client $hwnd 380 570
    Click-Client $hwnd 460 610
    Send-Key $hwnd 0x0D # Enter

    # DIMENSION: first point, second point, dimension-line point.
    Send-Key $hwnd 0x44 # D
    Click-Client $hwnd 560 600
    Click-Client $hwnd 700 600
    Click-Client $hwnd 560 560

    # TEXT: insertion point, WM_CHAR stream, Enter commit.
    Send-Key $hwnd 0x58 # X
    Click-Client $hwnd 740 520
    Send-Char $hwnd 0x43 # C
    Send-Char $hwnd 0x41 # A
    Send-Char $hwnd 0x44 # D
    Send-Key $hwnd 0x0D # Enter

    # COPY the selected text using two real canvas clicks.
    Send-Key $hwnd 0x59 # Y
    Click-Client $hwnd 740 520
    Click-Client $hwnd 780 500

    Write-Snapshot $hwnd 1 $beforeDelete
    $before = Get-Content -Raw -Path $beforeDelete

    if ($before -notmatch '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+0(?:\.0+)?\s+LINE\s+') {
        throw "GUI Line interaction did not persist a LINE entity"
    }
    if ($before -notmatch '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+0(?:\.0+)?\s+CIRCLE\s+') {
        throw "GUI Circle interaction did not persist a CIRCLE entity"
    }
    if ($before -notmatch '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+0(?:\.0+)?\s+POLY\s+') {
        throw "GUI Rectangle interaction did not persist a closed POLY entity"
    }
    if (($before | Select-String -Pattern '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+0(?:\.0+)?\s+POLY\s+' -AllMatches).Matches.Count -lt 2) {
        throw "GUI Polyline interaction did not persist an additional POLY entity"
    }
    if ($before -notmatch '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+0(?:\.0+)?\s+ARC\s+') {
        throw "GUI Arc interaction did not persist an ARC entity"
    }
    if ($before -notmatch '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+0(?:\.0+)?\s+DIM\s+') {
        throw "GUI Dimension interaction did not persist a DIM entity"
    }
    if (($before | Select-String -Pattern '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+0(?:\.0+)?\s+TEXT\s+' -AllMatches).Matches.Count -lt 2) {
        throw "GUI Text/Copy interaction did not persist two TEXT entities"
    }
    if ($before -notmatch '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+0(?:\.0+)?\s+HATCH\s+') {
        throw "GUI Hatch interaction did not persist a HATCH entity"
    }

    # New Layer must participate in History. Undoing it must also normalize
    # the active layer so subsequent drawing still works on layer 0.
    [GuiTestNative]::SendMessage($hwnd, 0x0111, [IntPtr]1008, [IntPtr]::Zero) | Out-Null
    Write-Snapshot $hwnd 3 $layerCreated
    $withLayer = Get-Content -Raw -Path $layerCreated
    if ($withLayer -notmatch '(?m)^L\s+\d+\s+"Layer 1"\s+') {
        throw "Layer history regression: New Layer was not persisted"
    }

    Send-CtrlKey $hwnd 0x5A # Ctrl+Z
    Write-Snapshot $hwnd 4 $layerUndone
    $withoutLayer = Get-Content -Raw -Path $layerUndone
    if ($withoutLayer -match '(?m)^L\s+\d+\s+"Layer 1"\s+') {
        throw "Layer history regression: Ctrl+Z did not remove New Layer"
    }

    Send-Key $hwnd 0x43 # C
    Click-Client $hwnd 820 260
    Click-Client $hwnd 840 260
    Write-Snapshot $hwnd 5 $afterUndoDraw
    $afterLayerUndoDraw = Get-Content -Raw -Path $afterUndoDraw
    if (($afterLayerUndoDraw | Select-String -Pattern '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+0(?:\.0+)?\s+CIRCLE\s+' -AllMatches).Matches.Count -lt 2) {
        throw "Layer history regression: drawing failed after undo removed active layer"
    }

    # Escape returns to Select. Select the known line midpoint and Delete it.
    Send-Key $hwnd 0x1B
    Click-Client $hwnd 410 260
    Send-Key $hwnd 0x2E

    Write-Snapshot $hwnd 2 $afterDelete
    $after = Get-Content -Raw -Path $afterDelete

    if ($after -match '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+0(?:\.0+)?\s+LINE\s+') {
        throw "GUI Select/Delete interaction failed to remove the line"
    }
    if ($after -notmatch '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+0(?:\.0+)?\s+CIRCLE\s+') {
        throw "Delete regression: Circle was lost while deleting Line"
    }
    if ($after -notmatch '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+0(?:\.0+)?\s+POLY\s+') {
        throw "Delete regression: Rectangle was lost while deleting Line"
    }

    # Force the same autosave path used by the 30-second timer.
    [GuiTestNative]::SendMessage($hwnd, 0x0113, [IntPtr]1, [IntPtr]::Zero) | Out-Null
    if (!(Test-Path $recoveryPath)) {
        throw "Autosave regression: recovery snapshot was not created"
    }

    # Exercise File -> Exit (WM_COMMAND 1002), confirm discard in the real
    # MessageBox, then prove the recovery snapshot is removed.
    [GuiTestNative]::PostMessage($hwnd, 0x0111, [IntPtr]1002, [IntPtr]::Zero) | Out-Null
    $dialog = [IntPtr]::Zero
    $dialogDeadline = (Get-Date).AddSeconds(10)
    do {
        Start-Sleep -Milliseconds 100
        $dialog = [GuiTestNative]::FindDialogForProcess([uint32]$proc.Id)
    } while ($dialog -eq [IntPtr]::Zero -and (Get-Date) -lt $dialogDeadline)

    if ($dialog -eq [IntPtr]::Zero) {
        throw "Exit regression: discard confirmation dialog did not appear"
    }

    [GuiTestNative]::SendMessage($dialog, 0x0111, [IntPtr]6, [IntPtr]::Zero) | Out-Null

    if (!$proc.WaitForExit(10000)) {
        throw "Exit regression: application did not close after discard confirmation"
    }
    if (Test-Path $recoveryPath) {
        throw "Exit regression: recovery snapshot remained after explicit discard"
    }

    Write-Host "GUI interaction regression test passed"
}
finally {
    if (!$proc.HasExited) {
        Stop-Process -Id $proc.Id -Force
    }
    Remove-Item Env:ACP_GUI_TEST_SNAPSHOT_DIR -ErrorAction SilentlyContinue
}
