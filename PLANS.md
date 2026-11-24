# v8.0.1 Argo migration plan

## Objective

Create an untouched stock `v8.0.1` branch from Prusa's official tag and a
`v8.0.1-Argo-stable` branch containing the Argo changes from
`v6.9.1-Argo-stable`, rebased onto that stock release, reusing Prusa's new
filament-color system instead of carrying our own.

## Open questions

- None.

## Approved plan

- Verify Prusa's official `v8.0.1` tag (`077276b6e`); it is not a descendant of
  `v6.9.1` (merge base `a3d6bca45`, 764 upstream commits).
- Create `v8.0.1` directly at the tag without modifying the stock tree.
- Replay the Argo commits with `git rebase --onto`, dropping the 6.9.x repair
  commit and re-deriving its fixes against 8.0.1.
- Filament color (Owner, 2026-11-08): drop the printer side of the Filament
  Color Manager, keep only the PrusaLink API, dashboard picker and web tool
  mapping on top of Prusa's `FilamentTypeParameters::color`; `color_rgb` is the
  main API field; the palette is Prusa's printer palette.
- Build `coreone` and `coreone_indx` (8-tool INDX) in Docker with `-Werror`
  (new image `prusa-buddy-build:gcc13-v8`, `linux/amd64`).
- Follow-ups approved by Owner: MMU slots reachable over the filament API, a
  free color input in the web picker, history clean-up (README build-doc
  duplicate, unused system-compiler hunk, `start.md` churn), pre-commit hook
  repair on the Mac.

## Acceptance checks

- [x] `v8.0.1` is identical to `refs/tags/v8.0.1` (`077276b6e`).
- [x] Argo commits replayed; `git range-diff` against `v6.9.1-Argo-stable`
      shows only the adaptations listed in `ARGO-DEVELOPMENT.md`.
- [x] `coreone` builds green with `-Werror`.
- [x] `coreone_indx` builds green with `-Werror`.
- [ ] Flashed and exercised on the printer.

## Implementation status

- [ ] Not started
- [ ] In progress
- [x] Done (pending on-printer validation)

## Decisions

- **Filament Color Manager reduced to its PrusaLink layer.** Prusa 8.0 stores,
  prompts for and edits a per-tool filament color itself but exposes nothing
  over PrusaLink. About 525 lines of ours left Prusa's preheat / pause /
  config_store / `color.cpp` files. Colors saved under 6.9.1-Argo do not carry
  over.
- **Z-align on G28 and the Z endstop wizard use Prusa's `auto_calib_Z()`**
  (screen-less, backs off on an obstruction) instead of our `show_ui` patches.
- **INDX "color a docked tool on insertion" screen dropped**: Prusa has no
  color for filament that is not loaded.

## Handoff

- Date: 2026-11-24
- Speed-aware phase stepping calibration for INDX: closed as far as the speed model goes.
  Two calibrations on the printer (plan [`docs/planning/phase-stepping-speed-aware-calibration.md`](docs/planning/phase-stepping-speed-aware-calibration.md),
  sections "First printer run" and "Second calibration with the log") gave the model nothing
  usable: no phase slope reached its gate or 2 σ. Owner chose to take the speed sweep out
  (`8b6146c17`): the wizard tunes and verifies each harmonic, about 4–5 min; the model code
  stays, not called. User doc [`doc/phase-stepping-speed-model.md`](doc/phase-stepping-speed-model.md).
- Firmware file: `firmware/COREONE_INDX_v8.0.1-Argo-8b6146c17.bbf` (main checkout, not in git;
  screen version `8.0.1+17171`). Flashing it needs no recalibration; the current calibration
  stays valid. Way back: `firmware/COREONE_INDX_v8.0.1-Argo-4b14c9e01.bbf`.
- Printer PrusaLink: `http://192.168.178.102` (Safari holds the login). The network log was
  switched off with `firmware/syslog-off.gcode` (empty `M334`); Owner confirms its prompt on
  the printer screen.
- Open, unexplained: idle motor interrupt mean 4.7 µs (19 % of the period, no stock
  baseline); motor B misses about 3000 transmissions per calibration; one "phase stepping off"
  reading at 14:56; X sweep repeats differing by 1.1–2.2 rad.
- Still open from the migration: Z-align on the first home, the Z endstop wizard and
  PrusaLink tool colors (unchanged from 2026-11-08); the same flash covers them.
- Open blockers:
  - none

## Notes

- Build with the Docker workflow in
  [`Firmware-Build-Instructions-Docker.md`](Firmware-Build-Instructions-Docker.md).
