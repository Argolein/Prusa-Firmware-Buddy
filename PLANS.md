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
- Built: speed-aware phase stepping calibration for INDX (design and commit list:
  [`docs/planning/phase-stepping-speed-aware-calibration.md`](docs/planning/phase-stepping-speed-aware-calibration.md),
  user doc [`doc/phase-stepping-speed-model.md`](doc/phase-stepping-speed-model.md)). Unit tests
  and the `coreone_indx -Werror` build are green; three fresh-eyes verifications passed.
  Firmware file `firmware/COREONE_INDX_v8.0.1-Argo-b8bb358d5.bbf` (main checkout, not in git);
  way back: `firmware/COREONE_INDX_v8.0.1-Argo-4b14c9e01.bbf` (last build before this feature).
  Flashing changes nothing until the phase stepping calibration is run again.
- Nothing has run on a printer. Next, at the printer (Owner): flash, run the phase stepping
  calibration (≈ 12 min), read `/phase-stepping.html` in PrusaLink (interrupt cost, heap,
  verification, which harmonics got a speed model), then print
  `doc/phase-stepping-speed-model-sound-test.gcode` and compare the `M975 S0`/`S1` halves by ear.
- Still open from the migration: Z-align on the first home, the Z endstop wizard and
  PrusaLink tool colors (unchanged from 2026-11-08); the same flash covers them.
- Ready next (Owner, 2026-11-24): repair and gate the nhttp unit tests; format before the gate;
  unit tests for the speed record file and the speed-model runtime (detail: `.workflow/NEXT-SESSION-PROMPT.md`).
- Open blockers:
  - none (printer validation waits on Owner)

## Notes

- Build with the Docker workflow in
  [`Firmware-Build-Instructions-Docker.md`](Firmware-Build-Instructions-Docker.md).
