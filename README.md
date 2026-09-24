# Auto CAD Pro

Windows-first CAD/BIM application focused on a production-grade 2D foundation and an increasingly structured architectural 3D/BIM stack.

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
- transactional Block/Symbol creation from selected geometry plus repeated block insertion with project-aware Undo/Redo;
- Dimension and Hatch creation;
- Undo/Redo through the command history model;
- project New/Open/Save/Save As with unsaved-change protection and validated atomic replacement;
- DXF import/export subset with degenerate-input rejection and lossy-export warning;
- SVG export and vector PDF export;
- Windows CI build/test and downloadable x64 executable artifact;
- History-backed Text/Dimension/Hatch property editing;
- 30-second atomic autosave recovery snapshots with startup recovery;
- A4-A0 page setup, portrait/landscape and Fit/1:50/1:100/1:200 PDF layout workflow;
- 5,120-entity large-document regression coverage;
- versioned portable ZIP + SHA-256 and validated NSIS Windows installer;
- Unicode Japanese/Vietnamese PDF support.

Current architectural/3D foundation:
- geometry3d and semantic `model3d::Scene/Object3D`;
- real Architectural 3D GUI viewport with orbit/pan/zoom;
- Level, Wall, Slab, Column, Beam, hosted Door/Window openings and Roof generation;
- wall opening/frame generation and L/T/X wall junction support;
- multi-storey architectural scene building and scene revision caching;
- OBJ export and Blender bridge;
- explicit canonical length-unit model: existing project/architectural geometry remains millimeter-based for compatibility, while external bridges convert explicitly;
- Blender import regression prevents the former 3000 mm -> 3000 m scale failure;
- canonical BIM `ProjectModel` foundation with stable Element/Type/Level/Material IDs, parameters and host relationships;
- mesh remains a derived visualization/exchange representation rather than the intended canonical BIM source-of-truth.

The Windows GUI and 2D release foundation are in verified gates. Architectural 3D is now active development rather than a future-only phase. PDF embeds the bundled Noto Sans JP Unicode font with ToUnicode mapping for Japanese/Vietnamese text instead of relying on system fonts.

## Development gates

A feature is not considered implemented until it has:
1. a real core implementation (no UI-only stubs),
2. automated tests for normal and edge cases,
3. a reproducible Windows build,
4. a feature-status entry with evidence,
5. no known blocker-level regression.

For user-facing stateful tools, Undo/Redo, persistence and real GUI reachability are also required before production promotion.

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
- blocks/symbols with reusable references and transactional GUI creation/insertion
- layouts and printing
- recovery/autosave
- performance/stability hardening
- embedded Unicode fonts for reliable PDF text
- Windows installer and signing-ready release pipeline

### Phase 3 — BIM/3D Foundation — active
- canonical length/unit system and explicit exchange conversions
- canonical ProjectModel with stable BIM IDs and relationships
- BRep geometry-kernel abstraction and OpenCASCADE integration
- migrate Wall/Slab/Beam/Column/Roof generation from ad-hoc mesh generation to kernel-backed solids
- GPU 3D viewport and picking
- Project Browser and context-aware Properties
- architecture authoring tools backed by the canonical model

### Phase 4 — BIM interoperability/documentation
- IFC import/export
- model/paper space and sheets
- associative plans/elevations/sections/schedules
- DWG provider abstraction and compatibility diagnostics
- room/quantity engine

### Phase 5 — Extensibility/collaboration
- Command/Tool/Panel/Importer/Exporter registries
- Python scripting and plugin DLL support
- stable revisions/changesets, ownership and collaboration infrastructure

## Unit policy

The existing file format and architectural defaults are millimeter-based, so the canonical internal unit remains **millimeter** during the compatibility phase. This avoids silently invalidating existing saved projects and dimensions. All exchange boundaries must convert explicitly. Blender currently receives `0.001` scale from canonical units to meters and applies the transform to imported objects.

A future migration to meter-native canonical storage must be versioned at the project-format boundary rather than performed implicitly inside geometry code.

## Unicode vector export

SVG preserves UTF-8 text directly. PDF embeds the pinned Noto Sans JP runtime font as a Type0/CIDFontType2 resource with a ToUnicode map, covering Japanese, Vietnamese and common Latin text without depending on fonts installed on the target machine. Font source/license provenance is recorded under `third_party/noto` and `third_party/stb`.

## Open-source policy

FreeCAD source is LGPL2+ and may be reused under its license obligations. Auto CAD Pro may be independently branded and distributed, but copied/modified FreeCAD code remains subject to LGPL requirements. Third-party code is accepted only after license and provenance review.

OpenCASCADE integration is treated as a geometry-kernel dependency and will be isolated behind an Auto CAD Pro BRep abstraction so application/BIM code does not depend directly on kernel-specific types.

## Target

- OS: Windows 10/11 x64
- Primary toolchain: Visual Studio 2022 / MSVC, CMake, CTest
- 2D quality target: correctness, stability and construction-document workflow
- 3D/BIM quality target: semantic canonical data, kernel-backed geometry and associative documentation

## Windows release outputs

Every green Windows CI run produces:
- a runnable `AutoCADPro.exe` bundle with its Unicode font/license assets;
- a versioned portable ZIP;
- SHA-256 checksum for the portable package;
- a validated NSIS installer;
- a real GUI screenshot;
- GUI interaction document snapshots.

Tagged builds (`v*`) use `.github/workflows/release.yml` to build release artifacts and publish a GitHub Release. Authenticode signing is automatically enabled when `WINDOWS_CERT_BASE64` and `WINDOWS_CERT_PASSWORD` repository secrets are configured.

## Current release status

The main branch is protected by repeated core/workflow tests, real GUI launch/lifecycle tests, creation/annotation/edit-tool interaction matrices, large-document regression, atomic project-save regression, portable-package content validation and silent installer install/launch/uninstall validation. The 2D audit resolved the Unicode PDF blocker, page-setup persistence gap, layer-history gap, recovery-discard gap, hatch rendering/export inconsistency, silent DXF omission risk and missing installer runtime assets.

Architectural 3D now has a real GUI proof workflow. New BIM/3D work is gated by canonical-unit, ProjectModel and future BRep/kernel regression tests rather than being accepted as disconnected mesh-only features.
