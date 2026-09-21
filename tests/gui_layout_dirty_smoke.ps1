param(
    [Parameter(Mandatory = $true)]
    [string]$Exe
)

$ErrorActionPreference = "Stop"

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class GuiLayoutNative {
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
}
"@

$recoveryPath = Join-Path $env:LOCALAPPDATA "AutoCADPro\recovery.acp"

$cases = @(
    @{ Name = "paper-size"; Command = 1024; Pattern = '(?m)^PAGE\s+1\s+1\s+' },
    @{ Name = "orientation"; Command = 1025; Pattern = '(?m)^PAGE\s+0\s+0\s+' },
    @{ Name = "print-scale"; Command = 1026; Pattern = '(?m)^PAGE\s+0\s+1\s+10(?:\.0+)?\s+10(?:\.0+)?\s+10(?:\.0+)?\s+10(?:\.0+)?\s+1\s+50(?:\.0+)?\s*$' }
)

foreach ($case in $cases) {
    Remove-Item $recoveryPath -Force -ErrorAction SilentlyContinue

    $proc = Start-Process -FilePath $Exe -PassThru
    try {
        $deadline = (Get-Date).AddSeconds(20)
        do {
            Start-Sleep -Milliseconds 300
            $proc.Refresh()
        } while ($proc.MainWindowHandle -eq 0 -and !$proc.HasExited -and (Get-Date) -lt $deadline)

        if ($proc.HasExited -or $proc.MainWindowHandle -eq 0) {
            throw "Layout dirty regression ($($case.Name)): application did not create a main window"
        }

        $hwnd = $proc.MainWindowHandle
        [GuiLayoutNative]::SetForegroundWindow($hwnd) | Out-Null

        # Apply one persisted layout mutation to an otherwise clean document.
        [GuiLayoutNative]::SendMessage(
            $hwnd, 0x0111, [IntPtr]$case.Command, [IntPtr]::Zero) | Out-Null

        # Trigger the same autosave path used by the 30-second timer. It only
        # writes when g_app.dirty is true.
        [GuiLayoutNative]::SendMessage(
            $hwnd, 0x0113, [IntPtr]1, [IntPtr]::Zero) | Out-Null

        if (!(Test-Path $recoveryPath)) {
            throw "Layout dirty regression ($($case.Name)): persisted layout mutation did not mark project dirty"
        }

        $snapshot = Get-Content -Raw -Path $recoveryPath
        if ($snapshot -notmatch $case.Pattern) {
            throw "Layout dirty regression ($($case.Name)): recovery snapshot did not preserve the changed layout state"
        }
    }
    finally {
        if (!$proc.HasExited) {
            Stop-Process -Id $proc.Id -Force
        }
        Remove-Item $recoveryPath -Force -ErrorAction SilentlyContinue
    }
}

Write-Host "GUI layout dirty-state regression test passed"
