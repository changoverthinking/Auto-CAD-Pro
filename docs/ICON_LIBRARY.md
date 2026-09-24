# Auto CAD Pro full icon and function catalog

Source: project-owner supplied `auto-cad-pro-icons-full.zip`.

## Contract

- 553 icons are represented in the compiled Windows icon catalog.
- `manifest.tsv` is the canonical identity/source list.
- `feature-map.tsv` maps every visual capability to a feature-gate state.
- `implemented` means a real command/core path exists; `prototype`/`partial`/`planned` are never exposed as fake production capability.
- Runtime drawing is generated from a compact embedded 24x24 alpha-mask catalog, recolorable for active/inactive dark-theme states.
- Legacy hard-coded GDI glyphs remain a fallback while toolbar migration is completed.

## Packs

- `pack-a`: 21 icons
- `pack-aa`: 14 icons
- `pack-ab`: 12 icons
- `pack-ac`: 16 icons
- `pack-ad`: 13 icons
- `pack-ae`: 11 icons
- `pack-b`: 23 icons
- `pack-c`: 20 icons
- `pack-d`: 27 icons
- `pack-e`: 19 icons
- `pack-f`: 23 icons
- `pack-g`: 22 icons
- `pack-h`: 7 icons
- `pack-i`: 17 icons
- `pack-j`: 20 icons
- `pack-k`: 25 icons
- `pack-l`: 23 icons
- `pack-m`: 27 icons
- `pack-n`: 18 icons
- `pack-o`: 18 icons
- `pack-p`: 20 icons
- `pack-q`: 14 icons
- `pack-r`: 16 icons
- `pack-s`: 15 icons
- `pack-t`: 19 icons
- `pack-u`: 15 icons
- `pack-v`: 14 icons
- `pack-w`: 14 icons
- `pack-x`: 15 icons
- `pack-y`: 18 icons
- `pack-z`: 17 icons

## Promotion rule

An icon may become an enabled command only when its backend, real UI workflow, tests, persistence/undo requirements (where applicable), and Windows CI gate match the project feature policy.
