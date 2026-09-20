# Feature gates

Auto CAD Pro does not use UI visibility as proof of implementation.

## States

- **planned**: accepted into roadmap, no production code.
- **prototype**: code exists but correctness/stability is not fully proven.
- **verified**: automated tests pass on Windows CI for defined acceptance cases.
- **production**: verified plus real document workflow, undo/redo integration, persistence, performance and regression coverage.
- **blocked**: known defect prevents promotion.

## Promotion rule

A feature may be labeled production only when all of the following are true:

- implementation is reachable through the real application path;
- unit tests cover nominal, boundary and degenerate inputs;
- Windows CI is green;
- undo/redo does not corrupt the document;
- save/open round trip preserves the feature;
- no blocker or critical issue is open against it;
- upstream-derived code has a recorded source commit and license.

## Current matrix

| Feature | State | Evidence |
|---|---|---|
| line/polyline/circle/arc entities | verified | document integration + Windows core tests |
| move/copy/rotate/mirror/scale | verified | transform tests including property preservation |
| trim/extend/offset | verified | edit2d acceptance and edge-case tests |
| endpoint/midpoint/center/intersection snaps | verified | snap aperture and geometry tests |
| layers/entity properties | verified | layer visibility/locking/weights + persistence tests |
| blocks/block references | verified | block definition/reference integration tests |
| text/linear dimensions | verified | annotation document and persistence tests |
| hatches | verified | hatch entity and persistence tests |
| project save/open | verified | versioned round-trip tests |
| DXF ASCII import/export subset | verified | controlled 2D entity round-trip tests |
| drawing bounds/viewport fit | verified | bounds/aspect tests |
| page setup/print scale model | verified | A-series page and fixed-scale tests |
| Windows desktop GUI | prototype | canvas/grid, zoom/pan, object snap, Line/Circle/Polyline/Arc/Text, selection/grips, Move/Copy/Rotate/Scale/Mirror, Trim/Extend/Offset, Dimension/Hatch creation, layers/properties panel, Delete, Undo/Redo, project/DXF workflow, Text/Dimension/Hatch/Block rendering |
| Windows executable artifact | prototype | CI uploads AutoCADPro-Windows-x64 after push build/test using Node 24-native actions |
| constraints | planned | FreeCAD Sketcher upstream audit required |
| SVG export | prototype | visible 2D geometry, instantiated blocks, UTF-8 text, dimensions, hatches and effective line weights are serialized by the core exporter and exposed in the Windows GUI |
| PDF export | prototype | vector geometry and printable ASCII text are exported to PDF 1.4; unsupported Unicode text now rejects export instead of being silently replaced with question marks; embedded Unicode font support remains required |
| full GUI editing workflow | prototype | selection/transforms/edit2d/history/file workflow, object snap, Unicode text creation, layers panel and core entity rendering active; richer annotation styling and richer properties editing remain |

## 2026-09-19 GUI audit fixes

- Enforce layer locks for geometry edits and Delete.
- Reject new geometry on hidden/locked active layers instead of creating invisible or immediately uneditable entities.
- Reset the active layer after project open and DXF import to avoid stale layer IDs.
- Hide selection grips when the selected entity is not visible.
- Render effective entity/layer line weight instead of one fixed pen width.
- Add Shift+Enter closed-polyline completion so Hatch creation is reachable from the GUI.
- Upgrade checkout/upload CI actions to Node 24-native majors to remove runner deprecation warnings.
- Property and layer mutations now participate in command history so Ctrl+Z/Ctrl+Y covers entity layer assignment, visibility, line-weight overrides, and layer visibility/lock toggles.
- GUI Object Snap now resolves visible Line/Polyline/Circle/Arc geometry using the tested snap core, including segment intersections, and can be toggled with F3.
- Text creation is reachable in the real GUI: choose Text, click insertion point, type through the Windows Unicode character stream, and press Enter to commit through History.
- Object snap input resolution is kept separate from entity hit-testing so Select/Trim/Extend target acquisition uses the real cursor location instead of a snapped point.
- Text input converts the Windows Unicode character stream to UTF-8 on commit, including surrogate-pair-safe backspace handling.
- SVG export serializes visible 2D geometry, instantiated blocks, text, dimensions, hatches and effective line weights with XML escaping and drawing bounds-derived viewBox.
- Unsaved-change protection tracks command mutations, layer creation, DXF import, Undo and Redo; New/Open/Import/Exit/WM_CLOSE now require explicit discard confirmation when dirty, while successful project Save clears the dirty flag.
- Document-level Object Snap moved into acp_core with tests for intersections, hidden entities, centers and instantiated block geometry; intersection pairing is limited to segments already within the snap aperture to avoid global O(n²) mouse-move scans.
- Project file workflow now remembers the current .acp path, separates Save from Save As, maps Ctrl+S/Ctrl+Shift+S/Ctrl+O/Ctrl+N correctly, and delays discard confirmation until a chosen Open/Import file has been successfully parsed.
- Vector PDF export fits visible drawing geometry to the configured A4 landscape page model, preserves effective line weights, and emits blocks/dimensions/hatches/text.
- PDF text export no longer silently corrupts UTF-8 into '?' characters. Until a Unicode font is embedded/subset into the PDF, documents containing unsupported Unicode text fail export rather than producing misleading output.
