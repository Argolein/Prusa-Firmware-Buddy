# Z endstop calibration (Core One) — design record

**Status:** IMPLEMENTED — builds clean on `--preset coreone` with `-Werror` (2026-06-27). On-printer
verification pending.
**Branch:** `v6.6.0-Argo-stable`
**Targets:** Core One (`coreone` preset). Core One L / INDX share the geometry path but are
out of scope for this first pass — only build/flash `coreone`.

---

## Problem / goal

The Core One has three Z motors (front-left, rear-center, front-right) on a single Z driver.
They are squared mechanically by the existing **Z alignment calibration** (ram-to-top against
the Z endstops; `selftest::calib_Z` / `G162`). Because the printed Z endstops are never
perfectly equal, each printer has a residual gantry/bed skew. The user has fitted **adjustable
(screw) Z endstops** to trim each corner.

Today, verifying a screw adjustment requires running a full bed mesh — slow. We want a fast,
display-driven **"12 Z endstop calibration"** that homes, re-aligns Z, probes one point closest
to each motor, shows the three Z heights (+ spread), and offers **Try again / Quit** so the user
can iterate: adjust a screw → re-run → read the new spread.

---

## User-confirmed decisions (interview, 2026-06-27)

1. **Menu integration:** standalone screen with its own retry FSM (NOT a selftest-snake Action).
   Lives as a plain menu item in *Control → Calibrations & Tests*, labelled "12 Z endstop
   calibration". It does not participate in the snake's pass/fail + prerequisite system.
2. **Probe points:** derived from firmware geometry (see below), shown here for approval.
3. **Result readout:** absolute Z height (mm) for each of the 3 points **plus the max−min spread**.
4. **Retry scope:** every "Try again" re-runs the **full** sequence: home → Z-align → probe.
   (No separate nozzle-clean prompt for now.)
5. **Result guidance:** show the 3 values + spread, plus a **per-corner suggested screw turn**
   (see "Screw model" below). The lowest point is the **datum**; the higher corners are brought
   down to it so every adjustment is the same direction.
6. **Tolerance / green OK (revised 2026-06-27):** spread **≤ 0.10 mm = aligned**. This drives:
   - a persisted pass/fail (`config_store().selftest_result_z_endstop_calibration`) shown as a
     **green check on the menu entry when aligned, no icon otherwise** ("green or nothing");
   - an **"OK / out of tolerance"** line on the result screen next to the spread.
   Threshold is a named constant (`z_endstop_calib_tolerance_mm`), easy to retune.

## Screw model (turn hints)

Adjustable endstops use a standard **M3 coarse screw → 0.5 mm pitch**
(`z_endstop_screw_pitch_mm`). The bed seats on the screw tip along Z, so corner height moves
~**1:1** with the screw: `turns = ΔZ / 0.5 mm` (= `ΔZ_mm × 720°`). The result screen prints, for
each non-datum corner, `ΔZ` as a turn count. Direction is labelled **CW** based on the user's
build (CW = screw in = corner lower → lowers the higher corners onto the datum); the user confirms
the sign once on the first run (a known test turn also validates the 0.5 mm/turn × 1:1 model).
Because the Z home reference is a single point, the three readings are slightly coupled, so it
converges over 1–2 retry cycles rather than one perfect turn.

---

## Geometry — proposed probe points

Bed: `X_BED_SIZE = 250`, `Y_BED_SIZE = 220`. Probe = nozzle loadcell, `NOZZLE_TO_PROBE_OFFSET
= {0,0,0}`. Safe probe area = the mesh bounds (`Configuration_COREONE_adv.h:462-465`, GRID 21×21):

The `MESH_*` macros use **integer** bed-size constants for Core One (`X_BED_SIZE 250`,
`Y_BED_SIZE 220`) and integer division, so they evaluate to whole millimetres:
- `MESH_MIN_X = -(250/18)+15 = 2`, `MESH_MAX_X = 250-2 = 248`
- `MESH_MIN_Y = -(220/18)+15 = 3`, `MESH_MAX_Y = 220-3 = 217`

"Front" = `Y_min` (door side). Each point is the closest safely-probeable bed location to its
motor (defined symbolically in code so it tracks any bed/grid change):

| Motor        | Probe X            | Probe Y       | mm (coreone) |
|--------------|--------------------|---------------|--------------|
| Front-left   | `MESH_MIN_X`       | `MESH_MIN_Y`  | (2, 3)       |
| Front-right  | `MESH_MAX_X`       | `MESH_MIN_Y`  | (248, 3)     |
| Rear-center  | `(MIN_X+MAX_X)/2`  | `MESH_MAX_Y`  | (125, 217)   |

Defined symbolically from the `MESH_*` macros so they stay valid if bed/grid changes.
> Stale-data note: `Z_STEPPER_ALIGN_X/Y` in `Configuration_COREONE_adv.h:223-226` carry
> generic-Marlin values (`Y=290`, off a 220 mm bed) and `Z_STEPPER_AUTO_ALIGN` is disabled —
> do **not** use them as motor positions.

---

## Procedure (per iteration)

1. **Home XY** — `GcodeSuite::G28_no_parser(true, true, false)`.
2. **Z-align** — call `selftest::calib_Z(...)` **explicitly on every iteration** (ram-to-top
   against the adjustable endstops). Do **not** rely on `G28`'s auto-align: that branch is gated
   on `!z_auto_align_done` (`G28.cpp:384`) and the `z_auto_align_done` flag latches `true` after
   the first align (`G28.cpp:397`), only clearing on power-cycle / motors-off. So `G28` aligns at
   most once per power cycle — useless for an iterate-and-retry loop.
   Implementation must also ensure the step-3 Z-reference home does **not** re-enter that branch
   and double-align (e.g. home Z via a path that bypasses auto-align, or manage the flag).
3. **Home Z** — establish the Z reference (loadcell tap at the homing point) so probed heights
   are absolute machine-Z.
4. **Probe the 3 points** — `Probe::probe_at_point(pos, PROBE_PT_RAISE)` for each; returns the
   measured Z as a float, `NAN` on failure (`lib/Marlin/.../module/probe.h:57`). Bail to an error
   phase if any probe returns `NAN`.
5. **Compute spread** = max(z) − min(z).
6. **Show result** — display FL / RC / FR absolute Z + spread; radio buttons **Try again / Quit**.
7. **Try again →** loop to step 1. **Quit →** park and exit.

---

## Architecture / files (planned)

- **Procedure / orchestration:** a new marlin_stub G-code (free number, e.g. `G163` — verify it
  is unused) under `src/marlin_stubs/`, driving a new FSM. Reuses `G28_no_parser`, `calib_Z`,
  `probe_at_point`.
- **FSM phases:** new `PhasesZEndstopCalib { Homing, Aligning, Probing, ShowResult, Error }` in
  the client-FSM-types header, plus client-response wiring (Try again / Quit) and the
  marlin↔GUI dispatch tables.
- **Result data side-channel:** FSM `PhaseData` is only **4 bytes**
  (`include/common/fsm_base_types.hpp:19`), too small for 3 floats + spread. Publish results
  through a small dedicated struct read by the GUI frame — modeled on how selftest surfaces
  result data — rather than packing into the FSM payload.
- **GUI:** a frame derived from `SelftestFrameNamedWithRadio` (title + numeric rows + `RadioButtonFSM`)
  for the ShowResult screen; reuse existing in-progress/spinner frames for Homing/Aligning/Probing.
- **Menu item:** `MI_Z_ENDSTOP_CALIB` added to the `ScreenMenuSTSCalibrations` container
  (`src/gui/screen_menu_selftest_snake.hpp`), label "12 Z endstop calibration", `click()` enqueues
  the new G-code. (A plain item can coexist with the `MI_STS<Action>` items in that screen's tuple.)

---

## Open implementation questions (to resolve while coding, not blocking)

- Exact free G-code number, and whether to expose it as a user G-code at all or keep it
  GUI-internal.
- How to home Z for the reference (step 3) without re-triggering `G28`'s auto-align branch
  (bypass path vs. temporarily managing `z_auto_align_done`).
- Precise side-channel mechanism for the 3 results (shared global vs. existing selftest result
  buffer) — follow the closest existing pattern.
- Whether the nozzle needs heating to a low temp before loadcell tap for clean contact (selftest
  loadcell runs cold; likely fine).

---

## Implementation (as built)

Feature gated by a new `HAS_Z_ENDSTOP_CALIBRATION()` option (Core One family), driven by the
internal G-code **M1988**, with results passed to the GUI via the extended-FSM-data channel
(`ZEndstopCalibResult_t`, 16 B ≤ 37 B buffer — the 4-byte FSM payload was too small).

New files:
- `src/common/z_endstop_calibration_result.hpp` — `ZEndstopCalibResult_t` (3 heights + spread)
  plus the shared `z_endstop_calib_tolerance_mm` (0.10) and `z_endstop_screw_pitch_mm` (0.5).
- `src/marlin_stubs/M1988.{hpp,cpp}` — the procedure/FSM driver. The command moved from
  `M1987` during the 6.6.3 rebase because upstream assigned `M1987` to the heater selftest.
- `src/gui/screen_z_endstop_calibration.{hpp,cpp}` — screen + busy/result/error frames.

Edited:
- `ProjectOptions.cmake` — `set_feature_for_printers(HAS_Z_ENDSTOP_CALIBRATION ...)`.
- `client_fsm_types.h` — `ClientFSM::ZEndstopCalibration`.
- `client_response.hpp` — `PhasesZEndstopCalib` enum + `z_endstop_calib_responses`.
- `client_response.cpp` — dispatch entry.
- `marlin_stubs/gcode.cpp` — `case 1988`.
- `gui/dialogs/DialogHandler.cpp` — screen registration.
- `gui/screen_menu_selftest_snake.{hpp,cpp}` — `MI_Z_ENDSTOP_CALIB` injected into the
  Calibrations & Tests menu builder ("12 Z endstop calibration"); its `Loop()` shows the green
  `ok_color_16x16` icon when the persisted result is `passed`.
- `persistent_stores/.../config_store/store_definition.hpp` — `selftest_result_z_endstop_calibration`
  (`TestResult`, `ItemFlag::calibrations`); set by M1988 (passed if spread ≤ tolerance).
- `src/common/fsm_states.cpp`, `src/state/printer_state.cpp` — added the new enumerator to the
  three exhaustive `ClientFSM` switches (required by `-Werror=switch`).
- `src/marlin_stubs/CMakeLists.txt`, `src/gui/CMakeLists.txt` — register the new sources.

The Z-reference home (step 3) sets the global `z_auto_align_done = true` before `G28 Z` to bypass
the fork's auto-align branch, then restores it to `false`.

## Build / validation

Docker build per `Firmware-Build-Instructions-Docker.md`, `--preset coreone`, `-Werror`.
Manual on-printer check: run from the menu, confirm 3 probes + readout + retry loop, then a
real bed mesh to corroborate the reported spread.
