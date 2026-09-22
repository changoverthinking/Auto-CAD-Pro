# Compact Classic CAD Workspace

This workspace layout is intentionally optimized for drawing area and high-frequency CAD work.

## Production geometry

- Top tool strip: 28-34 px, adaptive to client height.
- Quick actions: New, Open, Save, Undo, Redo, Fit View.
- CAD tools: all 18 production drawing/edit tools remain on the same single-row toolbar.
- Permanent left rail: disabled in the default workspace.
- Right Layer/Properties panel: approximately 16% of client width, clamped to 160-360 px.
- Bottom status strip: 22 px.
- Canvas occupies all remaining client area.

At 1920x1080 client size this yields approximately:

- 33 px top toolbar (~3.1% of height)
- 307 px right panel (~16% of width)
- 0 px permanent left rail
- 22 px bottom status strip

This follows the space-efficiency pattern of classic CAD applications: persistent commands are dense and peripheral while the drawing canvas remains dominant.

## Interaction contract

- Every Tool enum value remains reachable from the top toolbar.
- Quick Undo and Redo use the same History state as keyboard Ctrl+Z / Ctrl+Y.
- Right panel rendering, hit testing and scrolling use the shared responsive layout metrics.
- Compact-window and quick-toolbar behaviors are covered by real Win32 GUI regressions.
