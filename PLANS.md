# v6.9.0 Argo migration plan

## Objective

Create an untouched stock `v6.9.0` branch from Prusa's official tag and a
`v6.9.0-Argo-stable` branch containing the Argo changes from
`v6.6.3-Argo-stable`, rebased onto that stock release.

## Open questions

- None.

## Approved plan

- Verify Prusa's official `v6.9.0` tag and its relationship to `v6.6.3`.
- Create `v6.9.0` directly at the verified upstream tag without modifying the
  stock tree.
- Replay the `v6.6.3-Argo-stable` commit series onto `v6.9.0`, excluding the
  explicitly non-portable `rebase repair` commit.
- Resolve conflicts commit-by-commit, preserving both upstream behavior and the
  intended Argo features, and create a new repair commit for the fixes that are
  specific to this base.
- Where upstream has absorbed an Argo feature, prefer the upstream
  implementation and record the decision.
- Build both `coreone` and `coreone_indx` in the documented Docker toolchain
  with `-Werror`.
- Update `ARGO-DEVELOPMENT.md` with the new branch model, migration notes and
  refreshed commit hashes, then push both branches to `origin`.

## Acceptance checks

- [x] `v6.9.0` is bit-identical to `refs/tags/v6.9.0`.
- [x] All 39 portable Argo commits are present on `v6.9.0-Argo-stable`.
- [x] The 6.6.3-specific repair commit is absent; a fresh 6.9.0 repair commit
      documents each re-derived fix and the feature commit it belongs to.
- [x] `coreone` builds green with `-Werror`.
- [x] `coreone_indx` builds green with `-Werror`.
- [ ] Flashed and exercised on the printer.

## Implementation status

- [ ] Not started
- [ ] In progress
- [x] Done (pending on-printer validation)

## Decisions

- **Dropped the 6.6.3 repair commit** (`3bf37e497`, subject `v6.6.3`) as the
  convention requires, and re-derived its still-relevant fixes against the 6.9.0
  APIs in `a52622186`. Its documentation content (`AGENTS.md`, `PLANS.md`,
  `ARGO-DEVELOPMENT.md`) was carried forward separately.
- **Adopted upstream's 1.5GT belt support** instead of the fork's hardcoded
  steps/mm. Prusa 6.9.0 added `HAS_15GT_BELTS()` with a Settings → Hardware
  switch that also invalidates the calibrations a belt change makes stale. Its
  1.5GT value (101.587) matches the fork's old 101.5873 to within
  0.0003 steps/mm.
- **Removed the Advanced Settings steps/mm block** (`7dd4bdcc9`). Upstream
  deleted `set_steps_per_unit_x/y` and moved the Z setter behind the debug-only
  `HAS_EXTRA_EXPERIMENTAL_SETTINGS`; a second, unsynchronised X/Y editor next to
  the belt switch could disagree with it. Advanced Settings keeps its three
  toggles.
- **Left `belts_15gt_installed` at its upstream default (off / 2GT)** rather than
  patching the config-store default, so the belt type stays an explicit,
  per-printer choice made through the UI that performs the calibration resets.
- **Kept `M1988`** for the Z endstop calibration wizard — still unused upstream.

## Handoff

- Date: 2026-08-20
- Completed this session:
  - Created stock `v6.9.0` at `refs/tags/v6.9.0` and pushed it to `origin`.
  - Replayed all 39 portable Argo commits onto it as `v6.9.0-Argo-stable`,
    resolving 15 conflicting commits.
  - Removed the superseded steps/mm menu and committed the 6.9.0 rebase repair.
  - Built `coreone` and `coreone_indx` green with `-Werror`.
  - Updated `ARGO-DEVELOPMENT.md`, `PLANS.md`, `AGENTS.md`.
- Next step:
  - Flash `coreone_release_boot.bbf`, then set
    **Settings → Hardware → "1.5GT Belts" → On** and confirm the warning. It
    resets XY homing calibration, the CoreXY grid origin, belt tuning and the
    X/Y axis selftest results — re-run those.
  - Re-verify the fork features on hardware, especially Adaptive PA, the Z
    endstop calibration wizard, the Filament Color Manager and the PrusaLink
    web tool mapping (the latter still needs a real multi-material print).
- Open blockers:
  - none

## Notes

- Build with the Docker/GCC13 workflow in
  [`Firmware-Build-Instructions-Docker.md`](Firmware-Build-Instructions-Docker.md).
