# Upstream component policy

## FreeCAD

- Repository: FreeCAD/FreeCAD
- Relevant 2D areas: Sketcher, Draft, TechDraw, import/export infrastructure
- License: LGPL-2.1 / LGPL2+ according to upstream licensing material
- Integration rule: never copy code without preserving provenance, notices and license obligations.
- Strategy: reuse mature algorithms/modules only where dependency boundaries are understood and testable. Do not wholesale-import unrelated 3D/UI code during the 2D phase.

### Candidate capabilities to study/reuse

1. Sketcher constraint model and solver integration
2. geometry editing semantics
3. robust undo/transaction behavior
4. DXF/import-export patterns
5. dimension/annotation behavior where applicable
6. test cases for geometric edge conditions

### Not yet imported

No FreeCAD source file has been copied into Auto CAD Pro at this foundation commit. This is intentional: each import must first have a provenance record and a defined adapter/test boundary.

## Acceptance checklist for any third-party source

- exact repository and commit SHA recorded
- source license recorded
- compatibility with Auto CAD Pro distribution model reviewed
- copied files retain required notices
- local modifications clearly marked
- adapter boundary exists
- tests prove the capability works independently of UI
- Windows CI passes
