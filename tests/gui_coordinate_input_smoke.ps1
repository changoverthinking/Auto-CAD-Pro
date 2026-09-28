param(
    [Parameter(Mandatory = $true)]
    [string]$Exe
)

$ErrorActionPreference = "Stop"

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class GuiCoordinateNative {
    [DllImport("user32.dll")]
    public static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")]
    public static extern IntPtr SendMessageString(
        IntPtr hWnd,
        uint Msg,
        IntPtr wParam,
        string lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern int GetWindowText(
        IntPtr hWnd,
        System.Text.StringBuilder lpString,
        int nMaxCount);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindowEx(
        IntPtr parent,
        IntPtr childAfter,
        string className,
        string windowName);

    [DllImport("user32.dll")]
    public static extern IntPtr GetDlgItem(IntPtr hDlg, int nIDDlgItem);

    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(
        EnumWindowsProc lpEnumFunc,
        IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(
        IntPtr hWnd,
        out uint processId);

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

function Send-Key([IntPtr]$hwnd, [int]$vk) {
    [GuiCoordinateNative]::SendMessage(
        $hwnd, 0x0100, [IntPtr]$vk, [IntPtr]::Zero) | Out-Null
}

function Send-Char([IntPtr]$hwnd, [int]$charCode) {
    [GuiCoordinateNative]::SendMessage(
        $hwnd, 0x0102, [IntPtr]$charCode, [IntPtr]::Zero) | Out-Null
}

function Click-Client([IntPtr]$hwnd, [int]$x, [int]$y) {
    $packed = (($y -band 0xFFFF) -shl 16) -bor ($x -band 0xFFFF)
    [GuiCoordinateNative]::SendMessage(
        $hwnd, 0x0201, [IntPtr]1, [IntPtr]$packed) | Out-Null
}

function DoubleClick-Client([IntPtr]$hwnd, [int]$x, [int]$y) {
    $packed = (($y -band 0xFFFF) -shl 16) -bor ($x -band 0xFFFF)
    [GuiCoordinateNative]::PostMessage(
        $hwnd, 0x0203, [IntPtr]1, [IntPtr]$packed) | Out-Null
}

function Set-PropertyDialog(
    [System.Diagnostics.Process]$proc,
    [string]$value) {

    $dialog = [IntPtr]::Zero
    $edit = [IntPtr]::Zero
    $deadline = (Get-Date).AddSeconds(10)
    do {
        Start-Sleep -Milliseconds 100
        $dialog =
            [GuiCoordinateNative]::FindDialogForProcess([uint32]$proc.Id)
        if ($dialog -ne [IntPtr]::Zero -and
            [GuiCoordinateNative]::IsWindowVisible($dialog)) {
            $edit = [GuiCoordinateNative]::GetDlgItem($dialog, 3003)
        }
    } while (
        ($dialog -eq [IntPtr]::Zero -or
         $edit -eq [IntPtr]::Zero -or
         ![GuiCoordinateNative]::IsWindowVisible($dialog)) -and
        (Get-Date) -lt $deadline)

    if ($dialog -eq [IntPtr]::Zero -or
        $edit -eq [IntPtr]::Zero) {
        throw "Property editor dialog/edit control did not become ready"
    }

    # WM_SETTEXT is a system message and Windows marshals it correctly
    # across process boundaries for a standard Edit control.
    Start-Sleep -Milliseconds 100
    [GuiCoordinateNative]::SendMessageString(
        $edit, 0x000C, [IntPtr]::Zero, $value) | Out-Null

    $ok = [GuiCoordinateNative]::FindWindowEx(
        $dialog,
        [IntPtr]::Zero,
        "Button",
        "OK")
    if ($ok -eq [IntPtr]::Zero) {
        throw "Property editor did not contain an OK button"
    }

    # BM_CLICK follows the same button notification path as a real user click.
    [GuiCoordinateNative]::SendMessage(
        $ok, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null

    # The property command that opened this modal dialog resumes only after
    # EndDialog returns. Wait until the dialog has disappeared so the main
    # window has time to finish applying the History command before another
    # command or snapshot is sent.
    $closeDeadline = (Get-Date).AddSeconds(10)
    do {
        Start-Sleep -Milliseconds 50
        $remaining =
            [GuiCoordinateNative]::FindDialogForProcess([uint32]$proc.Id)
    } while (
        $remaining -ne [IntPtr]::Zero -and
        (Get-Date) -lt $closeDeadline)

    if ($remaining -ne [IntPtr]::Zero) {
        throw "Property editor dialog did not close after OK"
    }
    Start-Sleep -Milliseconds 150
}

function Command-Property(
    [IntPtr]$hwnd,
    [System.Diagnostics.Process]$proc,
    [int]$command,
    [string]$value) {

    [GuiCoordinateNative]::PostMessage(
        $hwnd, 0x0111, [IntPtr]$command, [IntPtr]::Zero) | Out-Null
    Set-PropertyDialog $proc $value
}

function Snapshot([IntPtr]$hwnd, [int]$id) {
    $path = Join-Path $PWD (
        "artifacts\gui-interaction-" + $id + ".acp2d")
    Remove-Item $path -Force -ErrorAction SilentlyContinue

    $result = [GuiCoordinateNative]::SendMessage(
        $hwnd,
        0x8000 + 42,
        [IntPtr]$id,
        [IntPtr]::Zero)

    if ($result.ToInt64() -ne 1 -or !(Test-Path $path)) {
        throw "Could not create property editor snapshot $id"
    }

    return Get-Content -Raw -Path $path
}

function Parse-InvariantDouble([string]$value) {
    return [double]::Parse(
        $value,
        [Globalization.CultureInfo]::InvariantCulture)
}


function Ctrl-Key([IntPtr]$hwnd, [int]$key) {
    [GuiCoordinateNative]::SetForegroundWindow($hwnd) | Out-Null
    [GuiCoordinateNative]::keybd_event(0x11,0,0,[UIntPtr]::Zero)
    Start-Sleep -Milliseconds 50
    Send-Key $hwnd $key
    [GuiCoordinateNative]::keybd_event(0x11,0,2,[UIntPtr]::Zero)
    Start-Sleep -Milliseconds 100
}
function Near([double]$actual,[double]$expected) {
    if ([Math]::Abs($actual-$expected) -gt 0.00000001) { throw "Coordinate mismatch: $actual != $expected" }
}
New-Item -ItemType Directory -Force -Path artifacts | Out-Null
$env:ACP_GUI_TEST_SNAPSHOT_DIR = (Join-Path $PWD "artifacts")
$proc = Start-Process -FilePath $Exe -PassThru
try {
    $deadline=(Get-Date).AddSeconds(20)
    do { Start-Sleep -Milliseconds 200; $proc.Refresh() }
    while($proc.MainWindowHandle -eq 0 -and !$proc.HasExited -and (Get-Date) -lt $deadline)
    if($proc.MainWindowHandle -eq 0 -or $proc.HasExited) { throw "No coordinate test window" }
    $hwnd=$proc.MainWindowHandle
    [GuiCoordinateNative]::SetForegroundWindow($hwnd) | Out-Null
    # Leave Object Snap enabled. Typed values must bypass snap and screen pixels.
    Send-Key $hwnd 0x4C
    [GuiCoordinateNative]::PostMessage($hwnd,0x0100,[IntPtr]0x75,[IntPtr]::Zero) | Out-Null # F6
    Set-PropertyDialog $proc '1000.125,2000.0625'
    Command-Property $hwnd $proc 1054 '@300.25,-40.125'
    $line=Snapshot $hwnd 810
    $match=[regex]::Match($line,'(?m)^E\s+\d+\s+\d+\s+1\s+0\s+0\s+LINE\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)')
    if(!$match.Success) { throw "Typed line missing" }
    $expected=@(1000.125,2000.0625,1300.375,1959.9375)
    for($i=0;$i -lt 4;$i++) { Near (Parse-InvariantDouble $match.Groups[$i+1].Value) $expected[$i] }
    Ctrl-Key $hwnd 0x5A
    if((Snapshot $hwnd 811) -match '(?m)^E\s') { throw "Typed line Undo failed" }
    Ctrl-Key $hwnd 0x59
    if((Snapshot $hwnd 812) -ne $line) { throw "Typed line Redo mismatch" }

    Send-Key $hwnd 0x43
    Command-Property $hwnd $proc 1054 '0,0'
    Command-Property $hwnd $proc 1054 '@2.5<90'
    $circle=Snapshot $hwnd 813
    $match=[regex]::Match($circle,'(?m)^E\s+\d+\s+\d+\s+1\s+0\s+0\s+CIRCLE\s+(\S+)\s+(\S+)\s+(\S+)')
    if(!$match.Success) { throw "Typed polar circle missing" }
    Near (Parse-InvariantDouble $match.Groups[1].Value) 0
    Near (Parse-InvariantDouble $match.Groups[2].Value) 0
    Near (Parse-InvariantDouble $match.Groups[3].Value) 2.5

    # Cancelling an input dialog must not create an entity or consume a point.
    Send-Key $hwnd 0x4C
    [GuiCoordinateNative]::PostMessage($hwnd,0x0100,[IntPtr]0x75,[IntPtr]::Zero) | Out-Null
    $deadline=(Get-Date).AddSeconds(10)
    do { Start-Sleep -Milliseconds 100; $dialog=[GuiCoordinateNative]::FindDialogForProcess([uint32]$proc.Id) }
    while($dialog -eq [IntPtr]::Zero -and (Get-Date) -lt $deadline)
    if($dialog -eq [IntPtr]::Zero) { throw "Cancel dialog missing" }
    [GuiCoordinateNative]::SendMessage($dialog,0x0111,[IntPtr]2,[IntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 150
    if((Snapshot $hwnd 814) -ne $circle) { throw "Cancel modified document" }
    # Mixed-type group edits must accept precise coordinates with Snap enabled.
    Ctrl-Key $hwnd 0x41
    Send-Key $hwnd 0x4D
    Command-Property $hwnd $proc 1054 '0,0'
    Command-Property $hwnd $proc 1054 '@0.125,-0.0625'
    $group=Snapshot $hwnd 815
    $match=[regex]::Match($group,'(?m)^E\s+\d+\s+\d+\s+1\s+0\s+0\s+LINE\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)')
    if(!$match.Success) { throw "Group line missing" }
    $expected=@(1000.25,2000,1300.5,1959.875)
    for($i=0;$i -lt 4;$i++) { Near (Parse-InvariantDouble $match.Groups[$i+1].Value) $expected[$i] }
    $match=[regex]::Match($group,'(?m)^E\s+\d+\s+\d+\s+1\s+0\s+0\s+CIRCLE\s+(\S+)\s+(\S+)\s+(\S+)')
    if(!$match.Success) { throw "Group circle missing" }
    Near (Parse-InvariantDouble $match.Groups[1].Value) 0.125
    Near (Parse-InvariantDouble $match.Groups[2].Value) -0.0625
    Near (Parse-InvariantDouble $match.Groups[3].Value) 2.5
    Ctrl-Key $hwnd 0x5A
    if((Snapshot $hwnd 816) -ne $circle) { throw "Precise group Undo mismatch" }
    Ctrl-Key $hwnd 0x59
    if((Snapshot $hwnd 817) -ne $group) { throw "Precise group Redo mismatch" }
    Write-Host 'Precise coordinate GUI regression passed'
}
finally {
    if(!$proc.HasExited) { Stop-Process -Id $proc.Id -Force }
    Remove-Item Env:ACP_GUI_TEST_SNAPSHOT_DIR -ErrorAction SilentlyContinue
}
