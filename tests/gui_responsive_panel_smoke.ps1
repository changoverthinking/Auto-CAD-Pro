param(
    [Parameter(Mandatory = $true)]
    [string]$Exe
)

$ErrorActionPreference = "Stop"

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class GuiResponsiveNative {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X, Y; }

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool MoveWindow(
        IntPtr hWnd, int X, int Y, int nWidth, int nHeight, bool bRepaint);

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll")]
    public static extern bool ClientToScreen(IntPtr hWnd, ref POINT point);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessage(
        IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern bool PostMessage(
        IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetClassName(
        IntPtr hWnd,
        System.Text.StringBuilder lpClassName,
        int nMaxCount);

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

function Pack-ClientPoint([int]$x, [int]$y) {
    return [IntPtr]((($y -band 0xFFFF) -shl 16) -bor ($x -band 0xFFFF))
}

function Click-Client([IntPtr]$hwnd, [int]$x, [int]$y) {
    [GuiResponsiveNative]::SendMessage(
        $hwnd, 0x0201, [IntPtr]1, (Pack-ClientPoint $x $y)) | Out-Null
}

function DoubleClick-Client([IntPtr]$hwnd, [int]$x, [int]$y) {
    if (![GuiResponsiveNative]::PostMessage(
        $hwnd, 0x0203, [IntPtr]1, (Pack-ClientPoint $x $y))) {
        throw "PostMessage failed for property double-click"
    }
}

function Send-Key([IntPtr]$hwnd, [int]$vk) {
    [GuiResponsiveNative]::SendMessage(
        $hwnd, 0x0100, [IntPtr]$vk, [IntPtr]::Zero) | Out-Null
}

function Send-Char([IntPtr]$hwnd, [int]$charCode) {
    [GuiResponsiveNative]::SendMessage(
        $hwnd, 0x0102, [IntPtr]$charCode, [IntPtr]::Zero) | Out-Null
}

function Send-Wheel(
    [IntPtr]$hwnd,
    [int]$clientX,
    [int]$clientY,
    [int]$delta) {

    $screen = New-Object GuiResponsiveNative+POINT
    $screen.X = $clientX
    $screen.Y = $clientY
    if (![GuiResponsiveNative]::ClientToScreen($hwnd, [ref]$screen)) {
        throw "ClientToScreen failed"
    }

    $lParam = Pack-ClientPoint $screen.X $screen.Y
    $wheelBits = ($delta -band 0xFFFF)
    $wParam = [IntPtr](($wheelBits -shl 16))
    [GuiResponsiveNative]::SendMessage(
        $hwnd, 0x020A, $wParam, $lParam) | Out-Null
}

New-Item -ItemType Directory -Force -Path artifacts | Out-Null
$env:ACP_GUI_TEST_SNAPSHOT_DIR = (Join-Path $PWD "artifacts")
$snapshotPath = Join-Path $PWD "artifacts\gui-interaction-91.acp2d"
Remove-Item $snapshotPath -Force -ErrorAction SilentlyContinue

$proc = Start-Process -FilePath $Exe -PassThru
try {
    $deadline = (Get-Date).AddSeconds(20)
    do {
        Start-Sleep -Milliseconds 300
        $proc.Refresh()
    } while ($proc.MainWindowHandle -eq 0 -and !$proc.HasExited -and (Get-Date) -lt $deadline)

    if ($proc.HasExited -or $proc.MainWindowHandle -eq 0) {
        throw "Responsive panel regression: application did not create a main window"
    }

    $hwnd = $proc.MainWindowHandle
    [GuiResponsiveNative]::SetForegroundWindow($hwnd) | Out-Null

    if (![GuiResponsiveNative]::MoveWindow($hwnd, 40, 40, 700, 540, $true)) {
        throw "Responsive panel regression: MoveWindow failed"
    }
    Start-Sleep -Milliseconds 300

    $rect = New-Object GuiResponsiveNative+RECT
    if (![GuiResponsiveNative]::GetClientRect($hwnd, [ref]$rect)) {
        throw "Responsive panel regression: GetClientRect failed"
    }

    $clientWidth = $rect.Right - $rect.Left
    $clientHeight = $rect.Bottom - $rect.Top
    if ($clientWidth -lt 640 -or $clientHeight -lt 450) {
        throw ("Responsive panel regression: compact client area is unexpectedly small: " +
            $clientWidth + "x" + $clientHeight)
    }

    $toolbarHeight = [Math]::Min(
        34, [Math]::Max(28, [Math]::Floor($clientHeight / 32)))
    $panelWidth = [Math]::Min(
        360, [Math]::Max(
            160, [Math]::Floor(($clientWidth * 16) / 100)))
    $panelLeft = $clientWidth - $panelWidth
    $panelHeight = $clientHeight - 22 - $toolbarHeight

    1..10 | ForEach-Object {
        [GuiResponsiveNative]::SendMessage(
            $hwnd, 0x0111, [IntPtr]1008, [IntPtr]::Zero) | Out-Null
    }

    Send-Wheel $hwnd ($panelLeft + 60) ($toolbarHeight + 44) -120
    Click-Client $hwnd ($panelLeft + 70) ($toolbarHeight + 44)

    Send-Key $hwnd 0x4C
    Click-Client $hwnd 180 180
    Click-Client $hwnd 300 180

    [GuiResponsiveNative]::SendMessage(
        $hwnd, 0x8000 + 42, [IntPtr]91, [IntPtr]::Zero) | Out-Null
    if (!(Test-Path $snapshotPath)) {
        throw "Responsive panel regression: snapshot was not created"
    }
    $snapshot = Get-Content -Raw -Path $snapshotPath
    if ($snapshot -notmatch '(?m)^E\s+\d+\s+4\s+1\s+0\s+0(?:\.0+)?\s+LINE\s+') {
        throw "Responsive panel regression: layer scrolling/click hit-test did not select layer 4"
    }

    Send-Key $hwnd 0x58
    Click-Client $hwnd 360 260
    Send-Char $hwnd 0x43
    Send-Char $hwnd 0x41
    Send-Char $hwnd 0x44
    Send-Key $hwnd 0x0D

    $propertyRows = 16
    $fixedHeight = 76
    $layerStride = 26
    $rowHeight = [Math]::Floor(
        ($panelHeight - $fixedHeight - $layerStride) / $propertyRows)
    $rowHeight = [Math]::Min(22, [Math]::Max(14, $rowHeight))
    $remaining = $panelHeight - $fixedHeight - ($rowHeight * $propertyRows)
    $visibleLayerRows = [Math]::Max(1, [Math]::Floor($remaining / $layerStride))
    $propertiesTop = $toolbarHeight + 32 + ($visibleLayerRows * 26) + 10
    $valuesTop = $propertiesTop + 34

    $keyWidth = [Math]::Min(
        100,
        [Math]::Max(48, [Math]::Floor(($panelWidth * 45) / 100)))
    $valueX = $panelLeft + 10 + $keyWidth + 12
    $linetypeY = $valuesTop + (10 * $rowHeight) + [Math]::Floor($rowHeight / 2)
    if ($linetypeY -ge ($clientHeight - 22)) {
        throw "Responsive panel regression: Linetype row still overflows the panel"
    }

    DoubleClick-Client $hwnd $valueX $linetypeY

    $dialog = [IntPtr]::Zero
    $dialogDeadline = (Get-Date).AddSeconds(5)
    do {
        Start-Sleep -Milliseconds 100
        $dialog = [GuiResponsiveNative]::FindDialogForProcess([uint32]$proc.Id)
    } while ($dialog -eq [IntPtr]::Zero -and (Get-Date) -lt $dialogDeadline)

    if ($dialog -eq [IntPtr]::Zero) {
        throw "Responsive panel regression: lower property row is not reachable"
    }

    [GuiResponsiveNative]::SendMessage(
        $dialog, 0x0111, [IntPtr]2, [IntPtr]::Zero) | Out-Null

    Write-Host "GUI responsive panel regression test passed"
}
finally {
    if (!$proc.HasExited) {
        Stop-Process -Id $proc.Id -Force
    }
    Remove-Item Env:ACP_GUI_TEST_SNAPSHOT_DIR -ErrorAction SilentlyContinue
}
