param(
    [Parameter(Mandatory = $true)]
    [string]$Exe
)

$ErrorActionPreference = "Stop"

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class GuiStyleNative {
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessage(
        IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")]
    public static extern IntPtr SendMessageString(
        IntPtr hWnd, uint Msg, IntPtr wParam, string lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern bool PostMessage(
        IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindowEx(
        IntPtr parent, IntPtr childAfter, string className, string windowName);

    [DllImport("user32.dll")]
    public static extern IntPtr GetDlgItem(IntPtr hDlg, int nIDDlgItem);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(
        EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(
        IntPtr hWnd, out uint processId);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetClassName(
        IntPtr hWnd, System.Text.StringBuilder lpClassName, int nMaxCount);

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
    [GuiStyleNative]::SendMessage(
        $hwnd, 0x0100, [IntPtr]$vk, [IntPtr]::Zero) | Out-Null
}

function Send-Char([IntPtr]$hwnd, [int]$charCode) {
    [GuiStyleNative]::SendMessage(
        $hwnd, 0x0102, [IntPtr]$charCode, [IntPtr]::Zero) | Out-Null
}

function Click-Client([IntPtr]$hwnd, [int]$x, [int]$y) {
    $packed = (($y -band 0xFFFF) -shl 16) -bor ($x -band 0xFFFF)
    [GuiStyleNative]::SendMessage(
        $hwnd, 0x0201, [IntPtr]1, [IntPtr]$packed) | Out-Null
}

function Set-PropertyDialog(
    [System.Diagnostics.Process]$proc,
    [string]$value) {

    $dialog = [IntPtr]::Zero
    $edit = [IntPtr]::Zero
    $deadline = (Get-Date).AddSeconds(10)
    do {
        Start-Sleep -Milliseconds 100
        $dialog = [GuiStyleNative]::FindDialogForProcess([uint32]$proc.Id)
        if ($dialog -ne [IntPtr]::Zero -and
            [GuiStyleNative]::IsWindowVisible($dialog)) {
            $edit = [GuiStyleNative]::GetDlgItem($dialog, 3003)
        }
    } while (
        ($dialog -eq [IntPtr]::Zero -or $edit -eq [IntPtr]::Zero) -and
        (Get-Date) -lt $deadline)

    if ($dialog -eq [IntPtr]::Zero -or $edit -eq [IntPtr]::Zero) {
        throw "Color/Linetype property dialog did not become ready"
    }

    [GuiStyleNative]::SendMessageString(
        $edit, 0x000C, [IntPtr]::Zero, $value) | Out-Null

    $ok = [GuiStyleNative]::FindWindowEx(
        $dialog, [IntPtr]::Zero, "Button", "OK")
    if ($ok -eq [IntPtr]::Zero) {
        throw "Color/Linetype property dialog did not contain OK"
    }

    [GuiStyleNative]::SendMessage(
        $ok, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null

    $closeDeadline = (Get-Date).AddSeconds(10)
    do {
        Start-Sleep -Milliseconds 50
        $remaining =
            [GuiStyleNative]::FindDialogForProcess([uint32]$proc.Id)
    } while (
        $remaining -ne [IntPtr]::Zero -and
        (Get-Date) -lt $closeDeadline)

    if ($remaining -ne [IntPtr]::Zero) {
        throw "Color/Linetype property dialog did not close"
    }
    Start-Sleep -Milliseconds 150
}

function Command-Property(
    [IntPtr]$hwnd,
    [System.Diagnostics.Process]$proc,
    [int]$command,
    [string]$value) {

    [GuiStyleNative]::PostMessage(
        $hwnd, 0x0111, [IntPtr]$command, [IntPtr]::Zero) | Out-Null
    Set-PropertyDialog $proc $value
}

function Snapshot([IntPtr]$hwnd, [int]$id) {
    $path = Join-Path $PWD (
        "artifacts\gui-interaction-" + $id + ".acp2d")
    Remove-Item $path -Force -ErrorAction SilentlyContinue

    $result = [GuiStyleNative]::SendMessage(
        $hwnd, 0x8000 + 42, [IntPtr]$id, [IntPtr]::Zero)

    if ($result.ToInt64() -ne 1 -or !(Test-Path $path)) {
        throw "Could not create Color/Linetype snapshot $id"
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
    } while (
        $proc.MainWindowHandle -eq 0 -and
        !$proc.HasExited -and
        (Get-Date) -lt $deadline)

    if ($proc.HasExited -or $proc.MainWindowHandle -eq 0) {
        throw "AutoCADPro did not create a window for Color/Linetype regression"
    }

    $hwnd = $proc.MainWindowHandle
    [GuiStyleNative]::SetForegroundWindow($hwnd) | Out-Null

    # Create a selected Text entity using the real keyboard/canvas path.
    Send-Key $hwnd 0x58
    Click-Client $hwnd 300 260
    Send-Char $hwnd 0x43
    Send-Char $hwnd 0x41
    Send-Char $hwnd 0x44
    Send-Key $hwnd 0x0D

    # Entity style editors: Color and Linetype.
    Command-Property $hwnd $proc 1040 "#FF0000"
    Command-Property $hwnd $proc 1041 "Center"

    $entityState = Snapshot $hwnd 61
    if ($entityState -notmatch '(?m)^EX\s+\d+\s+1\s+255\s+0\s+0\s+1\s+2\s*$') {
        Write-Host "ENTITY STYLE STATE:"
        Write-Host $entityState
        throw "GUI Color/Linetype regression: entity overrides did not persist"
    }

    # Create and style a real active layer.
    [GuiStyleNative]::SendMessage(
        $hwnd, 0x0111, [IntPtr]1008, [IntPtr]::Zero) | Out-Null
    Command-Property $hwnd $proc 1042 "#0078FF"
    Command-Property $hwnd $proc 1043 "Dashed"

    $layerState = Snapshot $hwnd 62
    if ($layerState -notmatch '(?m)^LX\s+2\s+0\s+120\s+255\s+1\s*$') {
        Write-Host "LAYER STYLE STATE:"
        Write-Host $layerState
        throw "GUI Color/Linetype regression: layer style did not persist"
    }

    # Clear entity overrides to prove ByLayer remains a first-class editor state.
    Command-Property $hwnd $proc 1040 ""
    Command-Property $hwnd $proc 1041 ""

    $byLayerState = Snapshot $hwnd 63
    if ($byLayerState -notmatch '(?m)^EX\s+\d+\s+0\s+255\s+255\s+255\s+0\s+0\s*$') {
        Write-Host "BYLAYER STYLE STATE:"
        Write-Host $byLayerState
        throw "GUI Color/Linetype regression: clearing overrides did not restore ByLayer state"
    }

    Write-Host "GUI Color/Linetype regression test passed"
}
finally {
    if (!$proc.HasExited) {
        Stop-Process -Id $proc.Id -Force
    }
    Remove-Item Env:ACP_GUI_TEST_SNAPSHOT_DIR -ErrorAction SilentlyContinue
}
