# Stability and regression policy

Auto CAD Pro treats recurring defects as process failures, not isolated bugs.

## Definition of done for a defect

A defect is closed only when all of these exist:

1. **Reproduction** — a deterministic scenario that demonstrates the failure.
2. **Root cause** — the exact violated invariant, data-flow error, state error, API misuse, or integration mismatch.
3. **Minimal correction** — fix the responsible layer instead of adding compensating UI behavior.
4. **Regression coverage** — an automated test that fails before the fix and passes after it.
5. **Integration proof** — Windows build, CTest and real GUI launch are green.
6. **Persistence/undo proof** — when the feature mutates the drawing, save/open and undo/redo must preserve correctness.
7. **No stale-branch reintroduction** — integration branches must include current main before merge.

A workaround without a regression test is not considered a closed defect.

## Error classes

### P0 — data corruption / project loss
Examples:
- save succeeds but data is incomplete;
- undo/redo corrupts entity IDs or properties;
- DXF/project import silently changes geometry.

Required response:
- block release;
- add round-trip regression fixture;
- validate invalid/corrupt input handling;
- confirm failure is explicit rather than silent.

### P1 — feature produces incorrect CAD geometry
Examples:
- Trim/Extend chooses wrong intersection;
- snap resolves to hidden geometry;
- scale/mirror changes unsupported attributes.

Required response:
- isolate geometry/core logic from UI;
- add nominal + degenerate + boundary tests;
- test unit and integration path.

### P2 — GUI command/state defect
Examples:
- wrong tool remains active;
- hidden/locked layer still accepts edits;
- stale selection survives document replacement;
- input is snapped when hit-testing requires raw cursor coordinates.

Required response:
- define state transition;
- reset transient state at document/tool boundaries;
- add core test where possible and GUI lifecycle/integration proof.

### P3 — export/interoperability mismatch
Examples:
- unsupported Unicode is corrupted;
- unsupported DXF entity is accepted incorrectly;
- line weight or visibility is dropped.

Required response:
- reject unsupported data explicitly;
- never silently substitute or truncate;
- add round-trip/export content regression.

### P4 — build/release defect
Examples:
- only works on developer machine;
- runtime dependency missing;
- executable launches intermittently;
- branch based on stale main reintroduces fixed code.

Required response:
- reproduce in GitHub Windows runner;
- keep executable self-contained where possible;
- repeat tests/lifecycle checks;
- require current main ancestry for PR integration.

## Mandatory regression gates

Every integration into main must pass:

- Windows x64 configure and Release build;
- core CTest suite;
- core suite repeated 5 times to catch flaky state/order defects;
- real GUI launch and screenshot creation;
- GUI process lifecycle repeated 3 times;
- current-main ancestry check on pull requests.

## Branch discipline

- main is the only integration baseline.
- new work branches start from current main.
- old feature branches are evidence/history, not valid bases for new work.
- if an old branch contains useful code, cherry-pick/reimplement the minimal delta onto a fresh branch from main.
- never merge a diverged legacy branch wholesale into main.

## Feature-completion order

1. eliminate blocker and corruption defects;
2. stabilize current 2D tools and command state;
3. expand property/annotation editing;
4. autosave/recovery;
5. layouts/printing;
6. large-document performance;
7. installer/signing;
8. only then begin the 3D kernel phase.

## Root-cause log format

For every significant defect record:

- Symptom
- Reproduction
- Root cause
- Files/layers affected
- Corrective change
- Regression test
- CI evidence
- Residual limitation

This prevents the same defect from being rediscovered and patched differently in later updates.
