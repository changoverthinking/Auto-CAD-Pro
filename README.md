# Auto CAD Pro

Windows-first CAD application focused on a production-grade 2D workflow before any 3D expansion.

## Current executable

The repository builds a native Windows desktop application named `AutoCADPro.exe`.

Current Windows 2D release foundation:
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
- Windows CI build/test and downloadable x64 executable artifact;
- History-backed Text/Dimension/Hatch property editing;
- 30-second atomic autosave recovery snapshots with startup recovery;
- A4-A0 page setup, portrait/landscape and Fit/1:50/1:100/1:200 PDF layout workflow;
- 5,120-entity large-document regression coverage;
- versioned portable ZIP + SHA-256 and validated NSIS Windows installer;
- tag-triggered GitHub release workflow with optional Authenticode signing hooks.

The Windows GUI is now in the verified gate. The remaining explicit 2D export blocker is full Unicode PDF text embedding/subsetting; unsupported Unicode currently fails safely instead of producing corrupted text.

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
- Windows installer and signing-ready release pipeline

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

## Windows release outputs

Every green Windows CI run produces:
- `AutoCADPro.exe`;
- a versioned portable ZIP;
- SHA-256 checksum for the portable package;
- a validated NSIS installer;
- a real GUI screenshot;
- GUI interaction document snapshots.

Tagged builds (`v*`) use `.github/workflows/release.yml` to build release artifacts and publish a GitHub Release. Authenticode signing is automatically enabled when `WINDOWS_CERT_BASE64` and `WINDOWS_CERT_PASSWORD` repository secrets are configured.

## Current 2D release status

The main branch is protected by repeated core/workflow tests, real GUI launch/lifecycle tests, real GUI interaction regression, large-document regression and installer validation. 3D development should not begin until the remaining explicit 2D blocker—Unicode PDF font embedding/subsetting—is resolved or formally deferred.
