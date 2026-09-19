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
| Windows desktop GUI | prototype | canvas/grid, zoom/pan, Line/Circle/Polyline/Arc, selection/grips, Move/Copy/Rotate, Trim/Extend/Offset, layers panel, Delete, Undo/Redo, project/DXF workflow, Text/Dimension/Hatch/Block rendering |
| Windows executable artifact | prototype | CI uploads AutoCADPro-Windows-x64 after push build/test |
| constraints | planned | FreeCAD Sketcher upstream audit required |
| SVG/PDF export | planned | export pipeline not yet implemented |
| full GUI editing workflow | prototype | selection/transforms/edit2d/history/file workflow, layers panel and core entity rendering active; annotation creation and richer properties editing remain |
