# Adaptive Pressure Advance — planning notes (ARCHIVED)

> **Status: DONE / merged.** Implemented on the Argo fork — see commit `e1e470b7d`
> ("adaptive PA: avoid planner flush on M572 S/M900 K value changes") and the related
> calibration fix `a10f4b38e`. Kept for design rationale only; not an active plan.
> Current state: see [`ARGO-DEVELOPMENT.md`](../../ARGO-DEVELOPMENT.md).

## Objective
Implement Klipper-style **adaptive pressure advance** support for the Prusa Core One on Prusa-Buddy firmware (branch `v.6.5.3-Argo-adaptivePA`), so that OrcaSlicer's adaptive PA feature works without motion hiccups.

Concretely: a `M572 S<pa>` command that only changes the pressure-advance **value** (not the smoothing window) must **not** call `planner.synchronize()`. The new PA value must instead be queued and applied per-move from inside the existing precise_stepping / pressure_advance pipeline. The current Bartlett FIR smoothing window provides the inter-move blending automatically.

Target motion path: **phase stepping + input shaping** (the only stable path on Core One). Legacy stepper ISR is explicitly out of scope.

Reference: Klipper fix `c84d78f3f169bc5163d11b74837f9880b0b7dba4` and OrcaSlicer wiki `adaptive_pressure_advance_calib`.

## Open questions
- none (all four pre-implementation questions resolved on 2026-06-13; see Decisions).

## Approved plan

### Architecture (high level)
1. **Two M572 paths** in `M572_internal`:
   - **Hot path (S-only, no activation toggle)**: update a single shared "queued PA value", do **not** flush. Future moves emitted by the planner pick up this value when they are converted to move segments.
   - **Structural path (W changed, or first activation, or `PressureAdvanceDisabler`)**: keep current `planner.synchronize()` + `set_axis_e_config` flow.
2. **Per-move PA value storage**: extend `block_t` (planner buffer) and `move_t` (move segment queue) with a `float pressure_advance_value` field. Propagate gcode-thread → block → move so a G1 enqueued before an `M572` keeps its old PA value, and a G1 enqueued after picks up the new one. This guarantees correct ordering across the `planner.buffer_segment` → `append_move_segments_to_queue` boundary.
3. **Step generator reads per-move PA**: replace `PressureAdvance::pressure_advance_params.pressure_advance_value` reads in `pressure_advance.cpp` with the value stored on the current `move_t`. The Bartlett FIR window (`smooth_time`-driven) naturally blends the extruder-position contribution between moves with different PA values — this is exactly the smoothing Klipper relies on.
4. **Sticky PA step generator**: once PA has ever been activated in a session, keep `PRESSURE_ADVANCE_STEP_GENERATOR_E` set in `PreciseStepping::physical_axis_step_generator_types` for the rest of the session. A move with `pressure_advance_value == 0` then behaves like classic stepping (no PA contribution) inside the same generator. This removes the need to ever flush again after the first activation.

### File-level change list

#### 1. `lib/Marlin/Marlin/src/feature/pressure_advance/pressure_advance_config.hpp`
- Add namespace-scope API:
  - `float get_queued_value();` — atomic read of the shadow value.
  - `void set_queued_value(float v);` — atomic write; called by the M572 hot path. Must NOT flush, NOT assert empty queue.
  - `bool can_use_queued_path(const Config &new_cfg);` — predicate: returns `true` iff (a) `smooth_time` unchanged, (b) `pressure_advance` is being changed between two **non-zero** values, AND `e_axis_config.pressure_advance > 0` (i.e. generator already armed, sticky).
- Document that `Config::smooth_time` belongs to the structural path; `Config::pressure_advance` belongs to the hot path.

#### 2. `lib/Marlin/Marlin/src/feature/pressure_advance/pressure_advance_config.cpp`
- Add `static std::atomic<float> queued_value{0.f};` shadow.
- Implement `get_queued_value()` / `set_queued_value()` with `memory_order_relaxed`.
- In `set_axis_e_config(...)`:
  - Keep the existing flushing structural path (called only from M572 W-change, first activation, and `PressureAdvanceDisabler`).
  - When activating (`config.pressure_advance > 0.f`), also call `set_queued_value(config.pressure_advance)`.
  - Add comment: once the generator bit is set, we do not clear it on `pressure_advance == 0` anymore during a print (sticky). Clearing only happens at explicit reset (e.g. `init()` on power-on).
- Loosen the `move_segment_queue_size() == 0` assertion to apply only to the structural path. Replace with a smaller invariant: "if `e_axis_config.smooth_time` changes, the queue must be empty".

#### 3. `lib/Marlin/Marlin/src/gcode/feature/pressure_advance/M572.cpp`
- Refactor `M572_internal(pressure_advance, smooth_time)`:
  ```
  if (only_S_changed && pressure_advance::can_use_queued_path(new_cfg)) {
      pressure_advance::set_queued_value(pressure_advance);
      // also update the cached e_axis_config.pressure_advance for reporting via M572 (no params)
      pressure_advance::update_reported_value(pressure_advance);  // new helper, no flush
      return;
  }
  // else: structural path (smooth_time changed OR first activation OR deactivation request)
  planner.synchronize();
  if (!planner.draining()) {
      pressure_advance::set_axis_e_config(new_cfg);
  }
  ```
- No change to the `M572` user-facing parser; just the internal dispatcher.

#### 4. `lib/Marlin/Marlin/src/feature/precise_stepping/common.hpp`
- Extend `move_t` (around line 72) with `float pressure_advance_value = 0.f;` placed right before `MoveFlag_t flags` to avoid disturbing `reference_cnt` alignment. Verify with `static_assert(sizeof(move_t) <= …)` only if a size constraint exists; otherwise just add it. **Size impact**: `MOVE_SEGMENT_QUEUE_SIZE = 64`, so +4B = +256B RAM. Negligible on STM32H7.

#### 5. `lib/Marlin/Marlin/src/module/planner.h`
- Add `float pressure_advance_value;` to `block_t` (alongside `extruder`). Initialize to `0.f` in `block_t` clear path.

#### 6. `lib/Marlin/Marlin/src/module/planner.cpp`
- In `_populate_block(...)` (or whichever path sets `block->extruder`), assign:
  ```
  block->pressure_advance_value = pressure_advance::get_queued_value();
  ```
  This is the **gcode-thread snapshot**: every G1 carries the PA value that was queued at the time it was enqueued.

#### 7. `lib/Marlin/Marlin/src/feature/precise_stepping/precise_stepping.cpp`
- In `append_move_segments_to_queue(block_t &block, …)` / `append_move_segment_to_queue(...)`:
  - When constructing each move segment from a block, copy `move.pressure_advance_value = block.pressure_advance_value`.
- No change to the step generator dispatch logic (line ~1580) — still gated by `physical_axis_step_generator_types`, which is now sticky-on after first activation.
- Verify `update_maximum_lookback_time()` is correct: lookback depends on `filter_total_time`, which depends only on `smooth_time` — unchanged by S-only updates. Add a comment to that effect.

#### 8. `lib/Marlin/Marlin/src/feature/pressure_advance/pressure_advance.cpp`
- In `pressure_advance_precalculate_parameters(...)` (line 33): replace `params.pressure_advance_value` (line 41) with `current_move.pressure_advance_value`.
- In `pressure_advance_step_generator_next_step_event(...)` (line 389), at the next-move start-pos correction (line 427): replace `PressureAdvance::pressure_advance_params.pressure_advance_value` with `next_move->pressure_advance_value`.
- Add `static_assert` or inline comment that all other params (`sampling_rate`, `filter`, `filter_total_time`, `filter_delay`) remain global because they are tied to `smooth_time`.

#### 9. `lib/Marlin/Marlin/src/gcode/feature/advance/M900.cpp`
- **Small semantic change**: M900 K must no longer overwrite `smooth_time` with the default `0.04`. Replace the current
  ```
  M572_internal(newK, default_config.smooth_time);  // line 75
  ```
  with
  ```
  M572_internal(newK, pressure_advance::get_axis_e_config().smooth_time);
  ```
  Effect: a runtime `M900 K<x>` only changes the PA value, leaving `smooth_time` at whatever the operator/start-gcode set it to. This makes M900 K **always hot path** during a print, regardless of the active smooth_time. Document the deviation from upstream Marlin in a brief comment.
- The result: M900 K and M572 S behave identically with respect to flushing (both queued, no flush). M900 still has no `W` parameter — operators wanting to change smoothing must use `M572 W`.

#### 10. (Out of scope, leave unchanged) `src/marlin_stubs/G425.cpp` + `PressureAdvanceDisabler`
- Calibration-only path; keeps the flushing semantics. Document this in a comment next to the disabler class.

### Target behavior + acceptance checks
- **A1.** `M572 S0.04` followed by 100 mm of continuous extrusion, then `M572 S0.06`, then more extrusion: motion is continuous, no audible/visual stutter, no `planner.synchronize()` call in the hot path (verify with a temporary `SERIAL_ECHO` or counter in `synchronize()`).
- **A2.** OrcaSlicer adaptive-PA calibration print runs without the per-segment hiccups currently observed.
- **A3.** A G1 enqueued before `M572` retains its old PA value when it becomes a move segment (ordering correctness). Verify with a unit-test-style scenario: enqueue G1 with PA=0.04, then M572 S0.08, then G1 with PA=0.08; inspect the two move_t entries.
- **A4.** `M572 W0.05` still flushes (structural path). `M572` with no params still reports the active `e_axis_config`.
- **A5.** Cold start: first `M572 S0.05` activates the generator via the structural path (queue is empty anyway, so the flush is a no-op). Subsequent `M572 S<x>` use the hot path.
- **A6.** `PressureAdvanceDisabler` (G425) continues to disable and restore PA correctly.

### Risk areas
- **Memory ordering across threads.** gcode parser writes the shadow; motion thread reads it in `_populate_block`. `std::atomic<float>` with `relaxed` is sufficient because the moves are themselves serialized by the planner queue; we only need word-atomicity, not happens-before across the value boundary.
- **`update_maximum_lookback_time()` invariance.** Verify lookback derives solely from `smooth_time` + `filter_length` and is independent of the per-move PA value. If a future refactor ties lookback to PA value, this assumption breaks.
- **`move_t` size growth.** +4B × 64 = +256B in RAM. Verify against the `precise_stepping` static allocations. No issue expected on STM32H7.
- **Sticky generator semantics during print abort / `quick_stop`.** Confirm that after an abort, `e_axis_config` and queue are in a consistent state before the next print starts. Likely already fine because the structural path runs at the next first M572.
- **Inactive moves (E-only retraction, no XY).** `is_pressure_advance_active(move)` returns false when E moves alone. With per-move PA, behavior is unchanged; the per-move value is simply ignored for inactive moves.
- **`block_t` size growth.** +4B; planner buffer is 32 blocks on Core One = +128B. Negligible.

### Rollback strategy
- All changes are localised to the PA + planner block/move data structures plus `M572_internal`. Reverting `M572.cpp` to call `planner.synchronize()` unconditionally restores the pre-patch behavior. The new `block_t::pressure_advance_value` and `move_t::pressure_advance_value` fields become unused but harmless.

## Implementation status
- [ ] Not started
- [ ] In progress
- [x] Done

Detailed task list (Preheat for Unloading):
- [x] Add `preheat_for_unloading` to config store (`store_definition.hpp`).
- [x] Add `MI_ADV_PREHEAT_FOR_UNLOADING` switch to GUI headers and implementation.
- [x] Add to `ScreenMenuAdvancedSettings` menu layout.
- [x] Modify `M702_unload` in `M701_2.cpp` to bypass preheat when disabled.
- [x] Modify `load_unload` in `M701_2.cpp` to allow cold extrude when preheat is disabled.
- [x] Run build tests to verify no syntax errors (compiled via Docker `utils/build.py`).

## Decisions
- The "Preheat before unloading" feature is user-configurable from the "Advanced settings" menu. It is enabled by default.
- If disabled, `M702` (unload) will bypass the preheat block completely and `load_unload` will temporarily disable `PREVENT_COLD_EXTRUSION` specifically for unloading, so the filament will retract cold. This correctly assumes the user relies on their slicer's end G-code to have moved the filament out of the melt zone safely.
- Used Docker compilation to verify that changes don't cause any compile errors on GCC 13.

## Handoff
- Agent: Gemini CLI
- Date: 2026-06-20
- Completed this session:
  - Addressed the user's request to make the "preheat before unload" feature optional via a toggle.
  - Implemented the config store flag, GUI toggle logic, and `M701_2.cpp` logic to skip preheating and allow cold extrusion during unload when disabled.
  - Verified compilation via Docker GCC 13.
- Stopped at:
  - Implementation is fully complete and verified to compile.
- Next step:
  - Flash the firmware onto the hardware to verify functionality works as intended.
- Open blockers:
  - none
- Decisions made this session:
  - Modified cold extrusion prevention condition on unloading to explicitly respect the new `preheat_for_unloading` config option, ensuring cold unloads are permitted without errors.

## Notes
- The firmware was successfully built via Docker.
- A previous issue with `GetIndex()` vs `value()` in the UI toggle implementation was fixed and now compiles correctly.
