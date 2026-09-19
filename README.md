# Auto CAD Pro

Windows-first CAD application focused on a production-grade 2D workflow before any 3D expansion.

## Development gates

A feature is not considered implemented until it has:
1. a real core implementation (no UI-only stubs),
2. automated tests for normal and edge cases,
3. a reproducible Windows build,
4. a feature-status entry with evidence,
5. no known blocker-level regression.

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
- performance/stability hardening
- Windows installer and signed release pipeline

### Phase 3 — 3D
Only starts after the 2D release gate is green. FreeCAD/OpenCascade-derived capabilities are evaluated here.

### Phase 4 — Blender interoperability
Only after the 3D kernel and data model are stable.

## Open-source policy

FreeCAD source is LGPL2+ and may be reused under its license obligations. Auto CAD Pro may be independently branded and distributed, but copied/modified FreeCAD code remains subject to LGPL requirements. Third-party code is accepted only after license and provenance review.

## Target

- OS: Windows 10/11 x64
- Primary toolchain: Visual Studio 2022 / MSVC, CMake, CTest
- First quality target: 2D correctness and stability
