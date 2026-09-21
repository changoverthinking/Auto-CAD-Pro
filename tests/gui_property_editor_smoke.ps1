param(
    [Parameter(Mandatory = $true)]
    [string]$Exe
)

$ErrorActionPreference = "Stop"

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class GuiPropertyNative {
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern bool GetClientRect(IntPtr hWnd, out RECT rect);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern bool SetWindowText(IntPtr hWnd, string lpString);

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
    [GuiPropertyNative]::SendMessage(
        $hwnd, 0x0100, [IntPtr]$vk, [IntPtr]::Zero) | Out-Null
}

function Send-Char([IntPtr]$hwnd, [int]$charCode) {
    [GuiPropertyNative]::SendMessage(
        $hwnd, 0x0102, [IntPtr]$charCode, [IntPtr]::Zero) | Out-Null
}

function Click-Client([IntPtr]$hwnd, [int]$x, [int]$y) {
    $packed = (($y -band 0xFFFF) -shl 16) -bor ($x -band 0xFFFF)
    [GuiPropertyNative]::SendMessage(
        $hwnd, 0x0201, [IntPtr]1, [IntPtr]$packed) | Out-Null
}

function DoubleClick-Client([IntPtr]$hwnd, [int]$x, [int]$y) {
    $packed = (($y -band 0xFFFF) -shl 16) -bor ($x -band 0xFFFF)
    [GuiPropertyNative]::PostMessage(
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
            [GuiPropertyNative]::FindDialogForProcess([uint32]$proc.Id)
        if ($dialog -ne [IntPtr]::Zero -and
            [GuiPropertyNative]::IsWindowVisible($dialog)) {
            $edit = [GuiPropertyNative]::GetDlgItem($dialog, 3003)
        }
    } while (
        ($dialog -eq [IntPtr]::Zero -or
         $edit -eq [IntPtr]::Zero -or
         ![GuiPropertyNative]::IsWindowVisible($dialog)) -and
        (Get-Date) -lt $deadline)

    if ($dialog -eq [IntPtr]::Zero -or
        $edit -eq [IntPtr]::Zero) {
        throw "Property editor dialog/edit control did not become ready"
    }

    # Allow WM_INITDIALOG to finish setting the initial value before replacing it.
    Start-Sleep -Milliseconds 100
    if (![GuiPropertyNative]::SetWindowText($edit, $value)) {
        throw "Could not set property editor value"
    }

    $verify = New-Object System.Text.StringBuilder 512
    [GuiPropertyNative]::GetWindowText($edit, $verify, $verify.Capacity) | Out-Null
    if ($verify.ToString() -ne $value) {
        throw ("Property editor value race: expected '" + $value +
               "', edit control contained '" + $verify.ToString() + "'")
    }

    $ok = [GuiPropertyNative]::FindWindowEx(
        $dialog,
        [IntPtr]::Zero,
        "Button",
        "OK")
    if ($ok -eq [IntPtr]::Zero) {
        throw "Property editor did not contain an OK button"
    }

    # BM_CLICK follows the same button notification path as a real user click.
    [GuiPropertyNative]::SendMessage(
        $ok, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null

    # The property command that opened this modal dialog resumes only after
    # EndDialog returns. Wait until the dialog has disappeared so the main
    # window has time to finish applying the History command before another
    # command or snapshot is sent.
    $closeDeadline = (Get-Date).AddSeconds(10)
    do {
        Start-Sleep -Milliseconds 50
        $remaining =
            [GuiPropertyNative]::FindDialogForProcess([uint32]$proc.Id)
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

    [GuiPropertyNative]::PostMessage(
        $hwnd, 0x0111, [IntPtr]$command, [IntPtr]::Zero) | Out-Null
    Set-PropertyDialog $proc $value
}

function Snapshot([IntPtr]$hwnd, [int]$id) {
    $path = Join-Path $PWD (
        "artifacts\gui-interaction-" + $id + ".acp2d")
    Remove-Item $path -Force -ErrorAction SilentlyContinue

    $result = [GuiPropertyNative]::SendMessage(
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
        throw "AutoCADPro did not create a window for property editor regression"
    }

    $hwnd = $proc.MainWindowHandle
    [GuiPropertyNative]::SetForegroundWindow($hwnd) | Out-Null

    $rect = New-Object GuiPropertyNative+RECT
    if (![GuiPropertyNative]::GetClientRect($hwnd, [ref]$rect)) {
        throw "GetClientRect failed"
    }

    # TEXT setup.
    Send-Key $hwnd 0x58
    Click-Client $hwnd 300 260
    Send-Char $hwnd 0x43
    Send-Char $hwnd 0x41
    Send-Char $hwnd 0x44
    Send-Key $hwnd 0x0D

    # First prove the direct menu -> native dialog -> History path one
    # property at a time, with a serialized snapshot after each step.
    Command-Property $hwnd $proc 1030 "EDITED-CAD"
    $contentState = Snapshot $hwnd 44
    if ($contentState -notmatch '"EDITED-CAD"') {
        $stage = [GuiPropertyNative]::SendMessage(
            $hwnd, 0x8000 + 43, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        $inputLength = [GuiPropertyNative]::SendMessage(
            $hwnd, 0x8000 + 44, [IntPtr]::Zero, [IntPtr]::Zero).ToInt64()
        $codes = @()
        if ($inputLength -gt 0 -and $inputLength -lt 256) {
            for ($i = 0; $i -lt $inputLength; $i++) {
                $codes += [GuiPropertyNative]::SendMessage(
                    $hwnd, 0x8000 + 45, [IntPtr]$i, [IntPtr]::Zero).ToInt64()
            }
        }
        Write-Host "CONTENT STATE:"
        Write-Host $contentState
        Write-Host ("PROPERTY STAGE: " + $stage)
        Write-Host ("PROPERTY INPUT LENGTH: " + $inputLength)
        Write-Host ("PROPERTY INPUT CODES: " + ($codes -join ","))
        throw "Property editor regression: Text Content menu edit did not apply"
    }

    Command-Property $hwnd $proc 1031 "7.25"
    $heightState = Snapshot $hwnd 45
    if ($heightState -notmatch 'TEXT\s+[^\r\n]*\s+7\.25\s+') {
        Write-Host "HEIGHT STATE:"
        Write-Host $heightState
        throw "Property editor regression: Text Height menu edit did not apply"
    }

    Command-Property $hwnd $proc 1032 "30"
    $rotationState = Snapshot $hwnd 46
    if ($rotationState -notmatch 'TEXT\s+[^\r\n]*\s+0\.523598') {
        Write-Host "ROTATION STATE:"
        Write-Host $rotationState
        throw "Property editor regression: Text Rotation menu edit did not apply"
    }

    Command-Property $hwnd $proc 1029 "0.70"
    $weightState = Snapshot $hwnd 47
    if ($weightState -notmatch '(?m)^E\s+\d+\s+\d+\s+1\s+1\s+') {
        Write-Host "WEIGHT STATE:"
        Write-Host $weightState
        throw "Property editor regression: Line Weight menu edit did not apply"
    }

    # Now prove the right-panel double-click route itself. Change content a
    # second time through the value cell.
    $clientWidth = $rect.Right - $rect.Left
    $clientHeight = $rect.Bottom - $rect.Top
    $toolbarHeight = [Math]::Max(
        34, [int]($clientHeight / 20))
    $panelWidth = [Math]::Max(
        180, [int](($clientWidth * 2) / 10))
    $panelLeft = $clientWidth - $panelWidth
    $panelBottom = $clientHeight - 26
    $layerY = $toolbarHeight + 32 + 26
    $propertiesTop = [Math]::Max(
        $layerY + 10,
        $toolbarHeight +
            [int](($panelBottom - $toolbarHeight) / 2))
    $valueX = $panelLeft + 120
    $contentY =
        $propertiesTop + 34 + (8 * 22) + 11

    DoubleClick-Client $hwnd $valueX $contentY
    Set-PropertyDialog $proc "PANEL-CAD"
    $panelState = Snapshot $hwnd 48
    if ($panelState -notmatch '"PANEL-CAD"') {
        Write-Host "PANEL STATE:"
        Write-Host $panelState
        throw "Property editor regression: right-panel double-click edit did not apply"
    }

    # Set the final content expected by the aggregate assertion.
    Command-Property $hwnd $proc 1030 "EDITED-CAD"

    $textState = Snapshot $hwnd 40
    $textPattern =
        '(?m)^E\s+\d+\s+\d+\s+1\s+1\s+([^\s]+)\s+' +
        'TEXT\s+[^\s]+\s+[^\s]+\s+([^\s]+)\s+([^\s]+)\s+' +
        '"EDITED-CAD"\s*$'
    $textMatch = [regex]::Match(
        $textState,
        $textPattern)

    if (!$textMatch.Success) {
        Write-Host $textState
        throw "Property editor regression: edited Text record was not found"
    }

    $weight = Parse-InvariantDouble $textMatch.Groups[1].Value
    $height = Parse-InvariantDouble $textMatch.Groups[2].Value
    $rotation = Parse-InvariantDouble $textMatch.Groups[3].Value

    if (
        [Math]::Abs($weight - 0.70) -gt 1e-9 -or
        [Math]::Abs($height - 7.25) -gt 1e-9 -or
        [Math]::Abs(
            $rotation - ([Math]::PI / 6.0)) -gt 1e-9) {
        throw "Property editor regression: Text numeric properties are incorrect"
    }

    # DIMENSION override.
    Send-Key $hwnd 0x44
    Click-Client $hwnd 420 500
    Click-Client $hwnd 560 500
    Click-Client $hwnd 420 460
    Command-Property $hwnd $proc 1033 "D-100"

    $dimState = Snapshot $hwnd 41
    $dimPattern =
        '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+[^\s]+\s+' +
        'DIM\s+[^\r\n]*\s+1\s+"D-100"\s*$'
    if ($dimState -notmatch $dimPattern) {
        Write-Host $dimState
        throw "Property editor regression: Dimension override did not persist"
    }

    # HATCH angle and spacing.
    Send-Key $hwnd 0x42
    Click-Client $hwnd 300 380
    Click-Client $hwnd 410 450
    Send-Key $hwnd 0x48
    Click-Client $hwnd 355 415

    Command-Property $hwnd $proc 1034 "30"
    Command-Property $hwnd $proc 1035 "2.5"

    $hatchState = Snapshot $hwnd 42
    $hatchPattern =
        '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+[^\s]+\s+' +
        'HATCH\s+"ANSI31"\s+([^\s]+)\s+([^\s]+)\s+0\s+'
    $hatchMatch = [regex]::Match(
        $hatchState,
        $hatchPattern)

    if (!$hatchMatch.Success) {
        Write-Host $hatchState
        throw "Property editor regression: edited Hatch record was not found"
    }

    $hatchAngle =
        Parse-InvariantDouble $hatchMatch.Groups[1].Value
    $hatchSpacing =
        Parse-InvariantDouble $hatchMatch.Groups[2].Value

    if (
        [Math]::Abs(
            $hatchAngle - ([Math]::PI / 6.0)) -gt 1e-9 -or
        [Math]::Abs($hatchSpacing - 2.5) -gt 1e-9) {
        throw "Property editor regression: Hatch numeric properties are incorrect"
    }

    # BLOCK scale and rotation.
    Send-Key $hwnd 0x4C
    Click-Client $hwnd 580 300
    Click-Client $hwnd 660 300
    [GuiPropertyNative]::SendMessage(
        $hwnd, 0x0111, [IntPtr]1027, [IntPtr]::Zero) | Out-Null

    Command-Property $hwnd $proc 1036 "1.5"
    Command-Property $hwnd $proc 1037 "45"

    $blockState = Snapshot $hwnd 43
    $blockPattern =
        '(?m)^E\s+\d+\s+\d+\s+1\s+0\s+[^\s]+\s+' +
        'BLOCKREF\s+\d+\s+[^\s]+\s+[^\s]+\s+' +
        '([^\s]+)\s+([^\s]+)\s*$'
    $blockMatch = [regex]::Match(
        $blockState,
        $blockPattern)

    if (!$blockMatch.Success) {
        Write-Host $blockState
        throw "Property editor regression: edited BlockRef record was not found"
    }

    $blockRotation =
        Parse-InvariantDouble $blockMatch.Groups[1].Value
    $blockScale =
        Parse-InvariantDouble $blockMatch.Groups[2].Value

    if (
        [Math]::Abs(
            $blockRotation - ([Math]::PI / 4.0)) -gt 1e-9 -or
        [Math]::Abs($blockScale - 1.5) -gt 1e-9) {
        throw "Property editor regression: Block numeric properties are incorrect"
    }

    Write-Host "GUI direct property editor regression test passed"
}
finally {
    if (!$proc.HasExited) {
        Stop-Process -Id $proc.Id -Force
    }
    Remove-Item Env:ACP_GUI_TEST_SNAPSHOT_DIR -ErrorAction SilentlyContinue
}
