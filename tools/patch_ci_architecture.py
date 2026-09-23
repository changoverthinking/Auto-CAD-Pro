from pathlib import Path

path = Path('.github/workflows/windows-ci.yml')
text = path.read_text(encoding='utf-8')
original = text


def replace_once(old: str, new: str, label: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise SystemExit(f'{label}: expected exactly one match, found {count}')
    text = text.replace(old, new, 1)


replace_once(
'''              [DllImport("user32.dll")]
              public static extern bool SetForegroundWindow(IntPtr hWnd);
''',
'''              [DllImport("user32.dll")]
              public static extern bool SetForegroundWindow(IntPtr hWnd);
              [DllImport("user32.dll")]
              public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
''',
'NativeWindow SendMessage')

replace_once(
'''            if (!(Test-Path "artifacts\\AutoCADPro-real-ui.png")) {
              throw "GUI screenshot was not created"
            }
''',
'''            if (!(Test-Path "artifacts\\AutoCADPro-real-ui.png")) {
              throw "GUI screenshot was not created"
            }

            # Exercise the real architectural 2D -> 3D pipeline through the
            # test-only window message compiled by ACP_ENABLE_GUI_TEST_HOOKS.
            # The hook creates a 6000 x 4000 sample plan using four wall lines
            # and a closed slab footprint, enables the roof, builds the scene,
            # switches the real app window to 3D and fits the camera.
            $architectural3DMessage = [uint32](0x8000 + 45)
            $architecturalResult = [NativeWindow]::SendMessage(
              $proc.MainWindowHandle,
              $architectural3DMessage,
              [IntPtr]::Zero,
              [IntPtr]::Zero)
            if ($architecturalResult -eq [IntPtr]::Zero) {
              throw "Architectural 3D GUI test hook failed"
            }
            Start-Sleep -Seconds 2

            $proc.Refresh()
            $rect3d = New-Object NativeWindow+RECT
            if (![NativeWindow]::GetWindowRect($proc.MainWindowHandle, [ref]$rect3d)) {
              throw "GetWindowRect failed for Architectural 3D"
            }
            $width3d = $rect3d.Right - $rect3d.Left
            $height3d = $rect3d.Bottom - $rect3d.Top
            $bitmap3d = New-Object System.Drawing.Bitmap $width3d, $height3d
            $graphics3d = [System.Drawing.Graphics]::FromImage($bitmap3d)
            try {
              $graphics3d.CopyFromScreen($rect3d.Left, $rect3d.Top, 0, 0, $bitmap3d.Size)
              $bitmap3d.Save((Join-Path $PWD "artifacts\\AutoCADPro-architectural-3d-ui.png"), [System.Drawing.Imaging.ImageFormat]::Png)
            }
            finally {
              $graphics3d.Dispose()
              $bitmap3d.Dispose()
            }
            if (!(Test-Path "artifacts\\AutoCADPro-architectural-3d-ui.png")) {
              throw "Architectural 3D GUI screenshot was not created"
            }
''',
'architectural screenshot capture')

replace_once(
'''            artifacts/AutoCADPro-real-ui.png
            artifacts/AutoCADPro-used-ui.png
''',
'''            artifacts/AutoCADPro-real-ui.png
            artifacts/AutoCADPro-used-ui.png
            artifacts/AutoCADPro-architectural-3d-ui.png
''',
'architectural screenshot artifact')

if text == original:
    raise SystemExit('CI patch produced no changes')
path.write_text(text, encoding='utf-8', newline='\n')
print('Architectural 3D screenshot CI patch applied')
