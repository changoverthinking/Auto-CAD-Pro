# Full icon / feature contract

This document is the implementation contract for the user-supplied Auto CAD Pro icon set.

## Source pack

- 553 unique icons.
- 31 packs.
- Canonical icon IDs are stored in `assets/icons/full/icon_ids.txt` and compiled into `acp::icons`.
- An icon is never proof that a feature exists.
- A command may be shown as enabled only when its `CommandDescriptor` is not `Planned` and `ui_reachable == true`.

## Current implementation mapping

The typed command registry currently maps 79 application commands to exact uploaded icon IDs. The mapping is regression-tested on Windows CI. Unmapped icons remain feature inventory and must not become active dead buttons.

## Pack contract

| Pack | Domain | Count | Implementation policy |
|---|---|---:|---|
| A | File / project lifecycle | 21 | finish project templates, recent files/projects, audit/purge/publish |
| AA | Views / view templates | 14 | after named views + viewport model |
| AB | Sheets / publishing | 12 | documentation phase |
| AC | Rooms / areas / quantities | 16 | BIM quantity phase |
| AD | Audit / diagnostics | 13 | stability tooling |
| AE | Performance / GPU | 11 | GPU viewport + profiling phase |
| B | Workspaces / panels | 23 | compact Auto CAD Pro workspace system |
| C | 2D drawing | 20 | highest-priority 2D parity |
| D | 2D modify | 27 | highest-priority 2D parity |
| E | Snaps / constraints | 19 | 2D precision phase |
| F | Layers / properties | 23 | layer/property parity |
| G | Annotation | 22 | dimension/text/table phase |
| H | Hatch | 7 | hatch editor/gradient/associativity |
| I | Blocks / references | 17 | block/xref phase |
| J | Layout / plot | 20 | model/paper space phase |
| K | 3D / BRep | 25 | OpenCASCADE modeling phase |
| L | 3D views | 23 | viewport/camera phase |
| M | Architecture | 27 | BIM authoring phase |
| N | BIM documentation | 18 | associative documentation |
| O | BIM project semantics | 18 | canonical ProjectModel phase |
| P | Import / export | 20 | data exchange providers |
| Q | Rendering | 14 | material/render phase |
| R | Project Browser | 16 | browser/tree phase |
| S | Collaboration | 15 | collaboration phase, last |
| T | Plugin / SDK / Python | 19 | extension platform phase |
| U | Status bar | 15 | drafting/status integration |
| V | Selection | 14 | selection parity |
| W | Measure / units | 14 | units + measurement phase |
| X | Settings / help | 15 | application settings phase |
| Y | BIM edit / hosting | 18 | host/join/profile editing |
| Z | Structure | 17 | structural authoring phase |

## Promotion rules

A feature progresses through `Planned -> Prototype -> Verified -> Production`.

### Planned

The feature exists only in the inventory/roadmap. Its icon may appear in a disabled discovery palette but never as an active command.

### Prototype

Core code exists. The command remains disabled in production UI unless the required workflow is safe and explicit.

### Verified

Automated acceptance tests pass on Windows CI for the defined scope. If `ui_reachable` is true, the command may be enabled.

### Production

In addition to Windows verification, the feature participates correctly in the real document workflow, history/Undo/Redo where applicable, persistence, packaging, and regression coverage.

## Delivery order

1. **2D parity:** C, D, E, F, G, H, V, W.
2. **Paper/documentation/browser:** J, N, R, AA, AB.
3. **3D exact modeling:** K, L, Q.
4. **BIM architecture/structure:** M, O, Y, Z, AC.
5. **Interchange:** P.
6. **Extension platform:** T.
7. **Collaboration:** S.

This order keeps the icon set and implementation synchronized without presenting non-functional buttons as completed CAD capabilities.
