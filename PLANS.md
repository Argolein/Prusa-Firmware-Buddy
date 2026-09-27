# v6.9.1 Argo migration plan

## Objective

Create an untouched stock `v6.9.1` branch from Prusa's official tag and a
`v6.9.1-Argo-stable` branch containing the Argo changes from
`v6.9.0-Argo-stable`, rebased onto that stock release.

## Open questions

- None.

## Approved plan

- Verify Prusa's official `v6.9.1` tag and its relationship to `v6.9.0`.
- Create `v6.9.1` directly at the verified upstream tag without modifying the
  stock tree.
- Replay the `v6.9.0-Argo-stable` commit series onto `v6.9.1` with
  `git rebase --onto`, resolving conflicts commit-by-commit and preserving both
  upstream behavior and the intended Argo features.
- Carry the `rebase repair` commit unchanged if none of its files changed
  upstream; otherwise re-derive it against 6.9.1.
- Build both `coreone` and `coreone_indx` in the documented Docker toolchain
  with `-Werror`.
- Update `ARGO-DEVELOPMENT.md` with the new branch model, migration note and
  refreshed commit hashes, then push both branches to `origin`.

## Acceptance checks

- [x] `v6.9.1` is identical to `refs/tags/v6.9.1` (`f1a123aba`).
- [x] All 44 Argo commits are present on `v6.9.1-Argo-stable`; `git range-diff`
      shows 43 patch-identical and one adapted (the `M1989` rename).
- [x] The repair commit is carried unchanged; none of its files changed between
      6.9.0 and 6.9.1.
- [x] `coreone` builds green with `-Werror`.
- [x] `coreone_indx` builds green with `-Werror`.
- [ ] Flashed and exercised on the printer.

## Implementation status

- [ ] Not started
- [ ] In progress
- [x] Done (pending on-printer validation)

## Decisions

- **Moved the Z endstop calibration wizard from `M1988` to `M1989`.** Upstream
  6.9.1 assigned `M1988` to the INDX gantry squareness wizard. Upstream keeps its
  number; the rename is folded into the Z-endstop feature commit because it is
  permanent, not specific to this base. The menu entry is unchanged.
- **Carried the 6.9.0 rebase repair commit unchanged** (6.6.1 precedent): 6.9.1
  is a direct descendant of 6.9.0 and touches none of the repaired files.
- The 6.9.0 decisions (upstream 1.5GT belt support, steps/mm menu removal, belt
  flag left at its 2GT default) still hold; see the 6.6.3 → 6.9.0 note in
  `ARGO-DEVELOPMENT.md`.

## Handoff

- Date: 2026-09-27
- Completed this session:
  - Created stock `v6.9.1` at `refs/tags/v6.9.1` and pushed it to `origin`.
  - Replayed all 44 Argo commits onto it as `v6.9.1-Argo-stable`; the only
    conflict was the `M1988` clash.
  - Built `coreone` and `coreone_indx` green with `-Werror`.
  - Updated `ARGO-DEVELOPMENT.md`, `docs/planning/z-endstop-calibration.md`,
    `PLANS.md`.
- Next step:
  - Flash `coreone_release_boot.bbf`. If the printer has 1.5GT belts and the
    switch is not already on, set **Settings → Hardware → "1.5GT Belts" → On**
    and re-run the calibrations it resets.
  - Re-verify the fork features on hardware, especially the Z endstop
    calibration wizard (now `M1989`), the TPU-safe INDX tool lock, Adaptive PA,
    the Filament Color Manager and the PrusaLink web tool mapping (the latter
    still needs a real multi-material print).
- Open blockers:
  - none

## Notes

- Build with the Docker/GCC13 workflow in
  [`Firmware-Build-Instructions-Docker.md`](Firmware-Build-Instructions-Docker.md).
