param(
    [Parameter(Mandatory = $true)]
    [string]$Exe
)

$ErrorActionPreference = "Stop"

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class GuiDockPanelNative {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X, Y; }

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr hWnd, ref POINT point);

    [DllImport("user32.dll")]
    public static extern bool SetCursorPos(int X, int Y);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessage(
        IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
}
"@

function Pack-Point([int]$x, [int]$y) {
    return [IntPtr]((($y -band 0xFFFF) -shl 16) -bor ($x -band 0xFFFF))
}

function Send-PointMessage(
    [IntPtr]$hwnd,
    [uint32]$message,
    [int]$x,
    [int]$y) {

    [GuiDockPanelNative]::SendMessage(
        $hwnd,
        $message,
        [IntPtr]1,
        (Pack-Point $x $y)) | Out-Null
}

function Move-CursorClient(
    [IntPtr]$hwnd,
    [int]$x,
    [int]$y) {

    $point = New-Object GuiDockPanelNative+POINT
    $point.X = $x
    $point.Y = $y
    if (![GuiDockPanelNative]::ClientToScreen($hwnd, [ref]$point)) {
        throw "Dock panel regression: ClientToScreen failed"
    }
    if (![GuiDockPanelNative]::SetCursorPos($point.X, $point.Y)) {
        throw "Dock panel regression: SetCursorPos failed"
    }
    Start-Sleep -Milliseconds 80
    Send-PointMessage $hwnd 0x0200 $x $y
}

function Get-PanelWidth([IntPtr]$hwnd) {
    return [int][GuiDockPanelNative]::SendMessage(
        $hwnd, 0x8000 + 43, [IntPtr]::Zero, [IntPtr]::Zero)
}

function Get-PanelState([IntPtr]$hwnd) {
    return [int][GuiDockPanelNative]::SendMessage(
        $hwnd, 0x8000 + 44, [IntPtr]::Zero, [IntPtr]::Zero)
}

$preferences = Join-Path $env:LOCALAPPDATA "AutoCADPro\ui.ini"
Remove-Item $preferences -Force -ErrorAction SilentlyContinue

$proc = Start-Process -FilePath $Exe -PassThru
try {
    $deadline = (Get-Date).AddSeconds(20)
    do {
        Start-Sleep -Milliseconds 250
        $proc.Refresh()
    } while ($proc.MainWindowHandle -eq 0 -and !$proc.HasExited -and (Get-Date) -lt $deadline)

    if ($proc.HasExited -or $proc.MainWindowHandle -eq 0) {
        throw "Dock panel regression: application did not create a main window"
    }

    $hwnd = $proc.MainWindowHandle
    [GuiDockPanelNative]::SetForegroundWindow($hwnd) | Out-Null

    $rect = New-Object GuiDockPanelNative+RECT
    if (![GuiDockPanelNative]::GetClientRect($hwnd, [ref]$rect)) {
        throw "Dock panel regression: GetClientRect failed"
    }

    $clientWidth = $rect.Right - $rect.Left
    $clientHeight = $rect.Bottom - $rect.Top
    $toolbarHeight = [Math]::Min(
        34, [Math]::Max(28, [Math]::Floor($clientHeight / 32)))

    $initialWidth = Get-PanelWidth $hwnd
    $initialState = Get-PanelState $hwnd
    if (($initialState -band 1) -eq 0 -or
        ($initialState -band 2) -eq 0 -or
        ($initialState -band 4) -ne 0) {
        throw "Dock panel regression: default state is not visible+pinned+expanded"
    }
    if ($initialWidth -lt 160) {
        throw "Dock panel regression: default panel width is too small"
    }

    # Hide / show through View command.
    [GuiDockPanelNative]::SendMessage(
        $hwnd, 0x0111, [IntPtr]1044, [IntPtr]::Zero) | Out-Null
    if ((Get-PanelWidth $hwnd) -ne 0) {
        throw "Dock panel regression: hide command did not release canvas width"
    }
    [GuiDockPanelNative]::SendMessage(
        $hwnd, 0x0111, [IntPtr]1044, [IntPtr]::Zero) | Out-Null
    if ((Get-PanelWidth $hwnd) -lt 160) {
        throw "Dock panel regression: show command did not restore the panel"
    }

    # Resize by dragging the real splitter.
    $panelLeft = $clientWidth - (Get-PanelWidth $hwnd)
    $dragY = $toolbarHeight + 120
    Send-PointMessage $hwnd 0x0201 $panelLeft $dragY
    Send-PointMessage $hwnd 0x0200 ($clientWidth - 300) $dragY
    Send-PointMessage $hwnd 0x0202 ($clientWidth - 300) $dragY

    $resizedWidth = Get-PanelWidth $hwnd
    if ([Math]::Abs($resizedWidth - 300) -gt 3) {
        throw "Dock panel regression: splitter resize did not produce ~300px panel"
    }

    # Explicit collapse / expand.
    [GuiDockPanelNative]::SendMessage(
        $hwnd, 0x0111, [IntPtr]1046, [IntPtr]::Zero) | Out-Null
    if ((Get-PanelWidth $hwnd) -ne 28) {
        throw "Dock panel regression: collapsed panel is not 28px"
    }
    [GuiDockPanelNative]::SendMessage(
        $hwnd, 0x0111, [IntPtr]1046, [IntPtr]::Zero) | Out-Null
    if ([Math]::Abs((Get-PanelWidth $hwnd) - 300) -gt 3) {
        throw "Dock panel regression: expand did not restore user width"
    }

    # Unpin => auto-hide tab. Hovering the tab expands it; mouse leave hides it.
    [GuiDockPanelNative]::SendMessage(
        $hwnd, 0x0111, [IntPtr]1045, [IntPtr]::Zero) | Out-Null
    $state = Get-PanelState $hwnd
    if (($state -band 2) -ne 0 -or (Get-PanelWidth $hwnd) -ne 28) {
        throw "Dock panel regression: unpin did not enter auto-hide tab state"
    }

    Move-CursorClient $hwnd ($clientWidth - 10) ($toolbarHeight + 80)
    Start-Sleep -Milliseconds 100
    $hoverState = Get-PanelState $hwnd
    if (($hoverState -band 8) -eq 0 -or
        [Math]::Abs((Get-PanelWidth $hwnd) - 300) -gt 3) {
        throw "Dock panel regression: hover did not expand unpinned panel"
    }

    # Move from the expanded panel into the canvas. Auto-hide is scoped to
    # the panel region, not merely to leaving the whole application window.
    Move-CursorClient $hwnd 100 ($toolbarHeight + 80)
    Start-Sleep -Milliseconds 50
    if ((Get-PanelWidth $hwnd) -ne 28) {
        throw "Dock panel regression: leaving the panel did not auto-hide it"
    }

    # Restore pinned expanded default width and verify preferences were written.
    [GuiDockPanelNative]::SendMessage(
        $hwnd, 0x0111, [IntPtr]1045, [IntPtr]::Zero) | Out-Null
    [GuiDockPanelNative]::SendMessage(
        $hwnd, 0x0111, [IntPtr]1047, [IntPtr]::Zero) | Out-Null

    if (!(Test-Path $preferences)) {
        throw "Dock panel regression: user preferences were not persisted"
    }
    $prefs = Get-Content -Raw -Path $preferences
    if ($prefs -notmatch 'right_panel_visible=1' -or
        $prefs -notmatch 'right_panel_pinned=1' -or
        $prefs -notmatch 'right_panel_collapsed=0' -or
        $prefs -notmatch 'right_panel_width=0') {
        throw "Dock panel regression: persisted panel state is incorrect"
    }

    Write-Host "GUI dockable right-panel regression test passed"
}
finally {
    if (!$proc.HasExited) {
        Stop-Process -Id $proc.Id -Force
    }
}
