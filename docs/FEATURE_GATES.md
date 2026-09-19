# Feature gates

Auto CAD Pro does not use UI visibility as proof of implementation.

## States

- **planned**: accepted into roadmap, no production code.
- **prototype**: code exists but correctness/stability is not proven.
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

## Initial matrix

| Feature | State | Evidence |
|---|---|---|
| Vec2/segment primitives | prototype | core tests added |
| segment intersection | prototype | normal/parallel/out-of-range tests |
| nearest-point projection | prototype | normal/degenerate tests |
| endpoint snap | prototype | core test |
| midpoint snap | prototype | core test |
| center snap | prototype | aperture test |
| line/polyline document entities | planned | not yet integrated |
| circle/arc entities | planned | not yet integrated |
| trim/extend/offset | planned | not yet implemented |
| constraints | planned | FreeCAD Sketcher upstream audit required |
| DXF import/export | planned | upstream audit required |
| GUI | planned | intentionally deferred until core behavior is real |
