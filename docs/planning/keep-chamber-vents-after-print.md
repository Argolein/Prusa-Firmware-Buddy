# Keep the chamber vents as they are after a print — planning notes

> **Status: implemented, build-validated (coreone_indx, -Werror); not yet tested on the
> printer.** Fork customization that removes one upstream step from the end of a print.
> Current state:
> [`ARGO-DEVELOPMENT.md`](../../ARGO-DEVELOPMENT.md).

## Objective
After a print ends, leave the chamber vents in whatever position they are in. Do not close
them. The next print already opens or closes them as its filament needs.

## Background — what upstream does
Prusa commit `7190ab828` "chamber: Close vents automatically at the end of a print"
(BFW-9080, first in `v6.9.0`, still in `v8.0.1`) added `Chamber::close_vents_after_print()`. It is called from
`pre_finalize_print()` in `src/common/marlin_server.cpp` (finished and aborted prints) and
closes the vents when they are open and vent control is set to automatic. The commit gives
no reason.

On the CORE One INDX this costs a whole tool sequence after every PLA/PETG print (vents are
opened at print start when the filament's chamber target is 45 °C or less,
`Chamber::manage_ventilation_state`):

1. `automatic_chamber_vents::execute_control()` homes X/Y precisely if they are not homed.
   An `M84 X Y` in the end G-code clears homing, so this homes almost every time.
2. It picks the tool from dock 4 (`VENT_DOCK`, T4), whose bowden operates the vent lever.
3. It moves the lever closed, then restores the previous tool state, which parks T4 again.

Seen on the printer: park, bed lowers, homing, pickup, park.

## Decision — the change
Remove the upstream step entirely (delete the old path, do not add a setting):

- `src/common/marlin_server.cpp`: delete the `#if HAS_CHAMBER_VENTS()` block that calls
  `buddy::chamber().close_vents_after_print();` in `pre_finalize_print()`.
- `src/feature/chamber/chamber.cpp`: delete `Chamber::close_vents_after_print()`.
- `src/feature/chamber/chamber.hpp`: delete its declaration and comment.

This is the inverse of `7190ab828` (21 lines, 3 files); `git revert 7190ab828` should apply
cleanly. Nothing else calls the function (checked with grep on `v8.0.1-Argo-stable`, 2026-11-12).

Behaviour after the change:
- `vent_state_` stays `open` after a PLA print. The next print's
  `manage_ventilation_state()` compares it with the target and moves the lever only when the
  state must change (for example before an ASA print).
- After a reboot `vent_state_` is unknown, so the first print actuates the lever as today.
- `vent_state_` is only what the firmware remembers. If the vents are closed by hand between
  prints, the next PLA print still believes they are open and neither opens them nor warns.
  Same as before `7190ab828`, but more likely now that the state stays `open`.
- Manual and off vent control are unaffected; manual mode no longer has an end-of-print path
  either way.

## Rebase note
This removes behaviour stock firmware added in 6.9.0. On a future rebase, check whether
upstream still closes the vents at print end (the call may have moved or gained callers);
if so, remove it again.

## Acceptance checks
- [x] `grep -rn close_vents_after_print src lib` returns nothing.
- [x] `coreone_indx` builds green with `-Werror` (Docker workflow).
- [ ] On the printer: a PLA print ends with the vents still open and no homing, pickup or
      park after the end G-code.
- [ ] On the printer: the following ASA print closes the vents at its start.

## Implementation status
- [ ] Not started
- [ ] In progress
- [x] Done (pending on-printer validation) — `77dab6229` (revert of `7190ab828`)
