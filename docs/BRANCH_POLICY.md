# Branch integration policy

## Authoritative baseline

`main` is the only integration and release baseline for Auto CAD Pro.

All new 2D maintenance or release work starts from current `main`. Pull requests targeting `main` must contain the current `main` ancestry and must pass the complete Windows CI release contract.

## Legacy development branches

Existing `dev/*` branches from earlier implementation phases are retained only as historical evidence. Their ahead/behind counts must not be interpreted as missing production functionality because many features were squash-merged or reimplemented cleanly on newer branches.

Legacy branches must never be merged wholesale into `main`. If a legacy branch appears to contain a useful delta, the delta must be verified against current `main` and reimplemented or selectively ported onto a new branch created from current `main`.

## Required integration path

1. Create a fresh branch from current `main`.
2. Make the minimal scoped change.
3. Pass source sanity and GUI reachability preflights.
4. Pass all core and workflow CTest suites, including repeated regression.
5. Pass real GUI launch/lifecycle and GUI interaction regressions.
6. Pass portable package and NSIS install/launch/uninstall validation.
7. Merge through a reviewed pull request only after all checks are green.

## Repository administration

GitHub branch protection/rulesets should require the Windows CI workflow on `main`, require pull requests, and block direct pushes. Repository administration is outside the permissions of the managed GitHub connection used by the automated development workflow, so this policy is additionally enforced in code/CI wherever repository permissions allow.
