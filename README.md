# Auto CAD Pro

Windows-first CAD application focused on a production-grade 2D workflow before any 3D expansion.

## Current executable

The repository builds a native Windows desktop application named `AutoCADPro.exe`.

Current GUI foundation:
- native Win32/GDI 2D workspace linked directly to `acp_core`;
- CAD grid, coordinate status, zoom around cursor and middle-mouse pan;
- interactive Line, Circle, Polyline, Arc and Text creation;
- Select with grips plus Move, Copy, Rotate, Scale and Mirror;
- Trim, Extend and Offset editing;
- endpoint/midpoint/center/intersection object snap with F3 toggle;
- layers/properties panel, visibility/lock controls and effective line weights;
- Dimension and Hatch creation;
- Undo/Redo through the command history model;
- project New/Open/Save/Save As with unsaved-change protection;
- DXF import/export;
- SVG export and vector PDF export;
- Windows CI build/test and downloadable x64 executable artifact.

The Windows GUI remains in the prototype gate because the production release still needs richer annotation/property editing, recovery/autosave, performance hardening, printing/layout workflow, installer/signing and wider real-document regression coverage.

## Development gates

A feature is not considered implemented until it has:
1. a real core implementation (no UI-only stubs),
2. automated tests for normal and edge cases,
3. a reproducible Windows build,
4. a feature-status entry with evidence,
5. no known blocker-level regression.

The authoritative maturity matrix is maintained in `docs/FEATURE_GATES.md`.

## Roadmap

### Phase 0 — Foundation
- Windows/MSVC CI
- deterministic geometry core
- command/undo transaction model
- feature maturity registry
- upstream/license ledger

### Phase 1 — 2D Core
- line, polyline, circle, arc
- trim/extend/offset
- move/copy/rotate/mirror/scale
- object snaps and tracking
- layers, line types, colors
- dimensions, text, hatches
- DXF import/export
- SVG/PDF export
- selection, grips, properties
- undo/redo and file recovery

### Phase 2 — 2D Production
- construction drawing workflows
- blocks/symbols
- layouts and printing
- recovery/autosave
- performance/stability hardening
- embedded/subset Unicode fonts for reliable PDF text
- Windows installer and signed release pipeline

### Phase 3 — 3D
Only starts after the 2D release gate is green. FreeCAD/OpenCascade-derived capabilities are evaluated here.

### Phase 4 — Blender interoperability
Only after the 3D kernel and data model are stable.

## Known export limitation

SVG is the current Unicode-safe vector export path. The PDF exporter intentionally rejects drawings containing text that cannot be represented by its current built-in PDF font instead of silently replacing UTF-8 characters with `?`. Proper embedded/subset Unicode font support is required before PDF text can be promoted beyond this prototype gate.

## Open-source policy

FreeCAD source is LGPL2+ and may be reused under its license obligations. Auto CAD Pro may be independently branded and distributed, but copied/modified FreeCAD code remains subject to LGPL requirements. Third-party code is accepted only after license and provenance review.

## Target

- OS: Windows 10/11 x64
- Primary toolchain: Visual Studio 2022 / MSVC, CMake, CTest
- First quality target: 2D correctness and stability
