# PLANS.md

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

Detailed task list:
- [x] Add `pressure_advance_value` field to `move_t` and `block_t`.
- [x] Implement `pressure_advance::queued_value` shadow + getters/setters.
- [x] Wire `block_t.pressure_advance_value = get_queued_value()` in `_populate_block` (both normal and raw-block paths).
- [x] Wire `move_t.pressure_advance_value = block.pressure_advance_value` in `append_move_segment_to_queue` (all three phase callers).
- [x] Refactor `M572_internal` to dispatch hot vs. structural path.
- [x] Switch `pressure_advance_precalculate_parameters` and the next-move start-pos correction to read per-move PA value.
- [x] Keep `PRESSURE_ADVANCE_STEP_GENERATOR_E` armed across non-zero PA changes; clear it on PA=0 (M572 S0 / PressureAdvanceDisabler).
- [x] Loosen assertion in `set_axis_e_config` (kept the queue-empty assertion since callers still flush before invoking it; sticky-on logic added so subsequent M572 S<x> bypass it via the hot path).
- [x] Patch `M900.cpp` to preserve current `smooth_time`.
- [x] Smoke build for Core One (Docker GCC13, `-Werror`, two builds — second after the Codex-driven semantics fix).
- [x] Manual verification on Core One hardware: feature works during real print, no observable hiccups, no regressions reported.
- [x] Remove now-dead `pressure_advance_params_t::pressure_advance_value` field (PR cleanup).

## Decisions
- The Bartlett FIR smoothing window is preserved as-is; it provides the per-move blending. No new low-pass filter is introduced.
- `smooth_time` (`W`) remains a structural parameter that requires queue drain; only `pressure_advance` (`S`) goes through the hot queued path. OrcaSlicer adaptive PA only emits `S`, so this covers the use case (confirmed 2026-06-13).
- PA step generator stays armed across non-zero PA value changes (the hot-path use case). **Going to PA value 0 is a structural disable**, not a hot-path update: the generator bit is cleared and the FIR lookback drops to zero. This is required so that `M572 S0` retains its documented "disable" semantics and so that `PressureAdvanceDisabler` (used by G28 / probe — see `assert(...is_active())` in `probe.cpp`) actually removes PA-induced motion latency. Re-activating after a disable (PA value 0 → >0) takes the structural path again. OrcaSlicer adaptive PA is unaffected: it varies PA between non-zero values and never emits S=0 mid-print, so the entire adaptive-PA hot loop remains flush-free.
- Per-move PA value is stored on both `block_t` (gcode-thread snapshot) and `move_t` (motion-thread carrier) to guarantee ordering across the planner→precise_stepping boundary.
- Queued PA shadow value uses `std::atomic<float>` with `memory_order_relaxed` (confirmed 2026-06-13). Word-atomicity on Cortex-M is sufficient; per-move ordering is enforced by the planner queue itself, not by the atomic.
- **M900 K must NOT overwrite `smooth_time`** (confirmed 2026-06-13). Current upstream behavior of resetting smooth_time to 0.04 on every M900 K is removed; instead M900 K reads the active smooth_time and only changes the PA value. This guarantees M900 K is always hot path during a print, matching M572 S semantics.
- Legacy stepper ISR (non phase-stepping) is **out of scope**.
- `PressureAdvanceDisabler` (used by G425 calibration, G28 homing, and `probe.cpp` — the latter contains an explicit `assert(...is_active())`) keeps its flushing semantics and now correctly drops the PA generator bit + FIR lookback during its scope, so homing/probe see classic E-stepping latency, not PA latency.

## Handoff
- Agent: Claude Code (Opus 4.7)
- Date: 2026-06-14
- Completed in this work item:
  - Analyzed M572 / pressure-advance / precise-stepping / phase-stepping code paths on branch `v.6.5.3-Argo-adaptivePA` and authored a file-level plan (above).
  - Implemented all 10 code changes across `block_t`, `move_t`, `pressure_advance_config`, `pressure_advance`, `precise_stepping`, `planner`, `M572`, and `M900`.
  - Two smoke builds with Docker GCC13 + `-Werror`. First build .bbf SHA256 `46418ad5…`; second build (after Codex fix) `f3dbfdef…`.
  - Addressed two correctness regressions surfaced by Codex review: an unconditional sticky-on broke `M572 S0` disable semantics and stopped `PressureAdvanceDisabler` from dropping PA lookback during homing/probe. Fixed by gating the hot path on `new value > 0` AND `!PressureAdvanceDisabler::is_active()`, and by making the structural path always clear the generator bit when PA value is 0.
  - PR cleanup: removed now-dead `pressure_advance_params_t::pressure_advance_value` field (PA amplitude lives on each `move_t` now).
  - Hardware-verified by user during real Core One print: adaptive PA works without observable hiccups.
- Stopped at:
  - Implementation complete, hardware-verified, PR-cleanup done. Awaiting user decision to commit / open PR.
- Next step:
  - Operator to review the final diff, then commit on `v.6.5.3-Argo-adaptivePA` and (optionally) open a PR upstream.
- Open blockers:
  - none.
- Decisions made:
  - Hot path covers non-zero ↔ non-zero PA value changes only. PA=0 (explicit M572 S0 or PressureAdvanceDisabler) always takes the structural path so the generator bit and FIR lookback are dropped — required for `M572 S0` documented semantics and for homing/probe latency.
  - `std::atomic<float>` with `memory_order_relaxed` for the per-move PA shadow.
  - `M900 K` no longer overwrites `smooth_time` with the default; it preserves whatever `smooth_time` is currently active, so `M900 K` during a print stays on the hot path.
  - OrcaSlicer adaptive PA only emits `M572 S<x>` with `x > 0`; `M572 W<x>` (smooth_time change) stays on the flushing structural path.
  - `can_use_queued_path` additionally guards against `PressureAdvanceDisabler::is_active()` for semantic consistency, even though the guard is normally unreachable from gcode-thread M572 handling.

## Notes
- The previous PLANS.md content (Advanced Settings menu / steps-per-mm / motor currents) is preserved in git history (commits up to `f62c2f66f`) and was marked Done by Codex on 2026-05-30. It has been replaced because it covered an unrelated, completed task.
- A smoke build on Core One should be requested before flashing — confirm with the user (per Notes guidance) which build preset/toolchain to use (Docker GCC13 was used for the previous Steps/mm change).
