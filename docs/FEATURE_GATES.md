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

Audit note (2026-09-25): historical labels below describe the listed acceptance
cases, not parity with a commercial CAD product. The evidence-based gap assessment
and minimum 8/10 release criteria are in `PRODUCT_QUALITY_PLAN.md`. Single-selection,
limited DXF exchange and incomplete canonical BIM integration remain product gaps.

| Feature | State | Evidence |
|---|---|---|
| line/polyline/circle/arc entities | verified | document integration + Windows core tests |
| move/copy/rotate/mirror/scale | verified | transform tests including property preservation |
| trim/extend/offset | verified | edit2d acceptance and edge-case tests |
| endpoint/midpoint/center/intersection snaps | verified | snap aperture and geometry tests |
| layers/entity properties | verified | layer visibility/locking/weights + persistence tests |
| blocks/block references | verified | transactional Create-from-Selected + Insert Active GUI workflow, project-aware Undo/Redo, Save/Open persistence and instantiated geometry regression coverage |
| text/linear dimensions | verified | annotation document and persistence tests |
| hatches | production | hatch entity/persistence tests plus shared angle/spacing-aware clipped pattern geometry used by GUI, SVG and PDF |
| project save/open | production | versioned round-trip + page settings persistence + validated temp/readback/parse/atomic-replace saves; forced temp-write failure preserves the previous project file |
| DXF ASCII import/export subset | verified | controlled Line/Circle/Arc/LWPolyline/Text round-trip, degenerate-input rejection and exported/skipped reporting; GUI warns before omitting unsupported visible entities |
| drawing bounds/viewport fit | verified | bounds/aspect tests |
| page setup/print scale model | production | A4-A0, portrait/landscape, Fit/1:50/1:100/1:200 workflow, fixed-scale PDF acceptance/rejection tests and real GUI controls |
| Windows desktop GUI | verified | native Win32 workspace is built on Windows CI; real-window tests cover launch/lifecycle plus Line/Circle/Rectangle/Polyline/Arc/Dimension/Text/Hatch/Copy/Select/Delete interactions and document snapshots |
| Windows executable artifact | production | CI builds a runnable app bundle, validates portable ZIP contents, silently installs NSIS, verifies bundled Unicode font/licenses, launches the installed app, silently uninstalls, and uploads app/portable/installer artifacts |
| constraints | planned | FreeCAD Sketcher upstream audit required |
| SVG export | verified | visible geometry, instantiated blocks, UTF-8 text, dimensions, line weights and angle/spacing-aware hatch output are covered by core tests and exposed in the Windows GUI |
| PDF export | production | vector geometry, fixed-scale layouts, line weights, patterned/solid hatches and Japanese/Vietnamese Unicode text are exported with bundled Noto Sans JP embedded as Type0/CIDFontType2 with ToUnicode mapping; corrupt fonts are rejected by regression tests |
| full GUI editing workflow | verified | History-backed geometry/property/annotation workflows, autosave/recovery, page setup, expanded real GUI interaction regression, repeated GUI lifecycle checks and Windows installer gate are active |

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

## 2026-09-21 final 2D release hardening

- Pull requests must contain current `main`; stale branch integration is blocked by CI.
- Core and workflow regression suites run repeatedly before GUI and packaging steps.
- Real GUI interaction snapshots now cover Line, Circle, Rectangle, Hatch, Arc, Polyline, Dimension, Text, Copy, Select and Delete.
- Autosave writes atomic recovery snapshots every 30 seconds for dirty drawings; startup recovery rejects corrupt snapshots and never overwrites the main project file.
- Layout workflow supports A4 through A0, portrait/landscape and Fit/1:50/1:100/1:200 PDF output.
- A 5,120-entity regression gate covers bounds, selection, object snap and project round-trip; current Windows CI completes this case in roughly 0.04-0.05 seconds per run.
- Windows release output includes executable metadata/manifest, portable ZIP, SHA-256 checksum and a validated NSIS installer.
- Tag-triggered release workflow can publish GitHub Releases and is Authenticode signing-ready when certificate secrets are configured.
- PDF Unicode production support is now resolved by the pinned bundled font path documented below.

## 2026-09-21 full 2D function audit

- New Layer now uses History through CreateLayerCommand; Undo removes the layer and Redo restores the same ID/state.
- Undo/Redo normalizes a stale active layer back to the default layer, preventing silent drawing failures after undoing layer creation.
- File -> Exit now removes autosave recovery data after an explicit discard, matching WM_CLOSE behavior.
- Page size, orientation, margins and fixed print scale now persist through project Save/Open and autosave Recovery; legacy ACP2D v1 files without a PAGE record still load with defaults.
- Hatch angle, spacing and solid/pattern state now drive shared clipped geometry across the real GUI, SVG and PDF rather than a fixed display-only brush.
- Real GUI edit-tool regression now covers Move, Undo/Redo, Copy, Rotate, Scale, Mirror, Offset, Trim and Extend in addition to the creation/annotation/delete interaction matrix.
- The Unicode PDF blocker is resolved with a pinned OFL Noto Sans JP runtime asset and pinned stb_truetype source/license provenance.

## 2026-09-21 final file/package integrity audit

- Project Save/Save As no longer truncates the destination directly: data is serialized, parsed, written to a same-directory temporary file, read back byte-for-byte, parsed again, then atomically replaces the destination on Windows with write-through semantics.
- A forced temporary-write failure regression proves the previous .acp file remains byte-identical.
- DXF import now rejects zero-length LINE entities, one-point open polylines and closed polylines with fewer than three points.
- DXF export reports supported vs omitted visible entities; the Windows GUI requires explicit confirmation before a lossy subset export.
- Portable ZIP content is expanded and validated in CI for the executable, Noto Sans JP font and license assets.
- NSIS installer now installs the Unicode font and third-party provenance/license files. CI silently installs it to a clean directory, verifies all runtime assets, launches the installed executable, and silently uninstalls it.
- Signing order is now application first, then packaging, then outer installer signing; therefore signed releases package the signed application binary rather than an unsigned inner executable.

## 2026-09-21 block/symbol workflow

- History now has a project-aware path for transactions that mutate both Document and BlockLibrary without weakening existing document-only commands.
- Create Block from Selected converts Line/Circle/Arc/Polyline geometry into a BlockDefinition and replaces the source with a BlockReference while preserving the source EntityId and entity properties.
- Undo restores the original primitive and removes the definition; Redo restores the same BlockId and reference.
- Insert Active Block is available through the Block menu, toolbar icon and K shortcut.
- Real Win32 GUI regression creates a block, inserts a second reference, undoes insertion, undoes block creation, then redoes both and verifies serialized project state.
