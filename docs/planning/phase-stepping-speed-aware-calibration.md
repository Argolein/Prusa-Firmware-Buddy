# Speed-aware phase stepping calibration (INDX) — plan

> **Status: APPROVED** (Owner, 2026-11-24: develop the feature so it works; no USB link to
> the printer). Implementation in progress. Scope: Core One INDX only (`HAS_INDX()`), branch
> `v8.0.1-Argo-stable`.

## Decisions

- 2026-11-24: fresh-eyes plan review (16 findings) folded in: CoreXY motor naming, per-axis
  ISR rate, int8 range, aliasing at the band edges, heap measurement, kₙ ambiguity,
  zero-crossing physics, live per-harmonic runtime option.
- 2026-11-24: second review round folded in: heap sampling point, table runtime as default
  (live `sin_lut` too slow), no harmonic applied twice, feed rates for test moves, alias
  check with the measured sample rate and m ≤ 16, |v| per direction.
- 2026-11-24 (Owner): no USB link is possible; develop the feature so it works. Replaces
  "measure before building, Phase 0 gates everything": the calibration measures and gates
  itself, data leave the printer over PrusaLink, validation is by ear from a G-code file.

## Objective

Make the two XY motors (A and B, see below) measurably quieter at print speed by improving how phase stepping is
calibrated **and** how the calibration data is used while printing:

1. calibrate the correction at several speeds instead of one, and apply it as a function of
   speed (today one table serves every speed);
2. verify every correction with an independent measurement before keeping it;
3. expose the measured data so its quality can be judged and tracked over time.

All of it has to fit the existing INDX hardware: no new sensors, no driver change.

## How stock phase stepping works (v8.0.1)

- **CoreXY:** the Core One is CoreXY (`Configuration_COREONE.h`: `COREXY`). Phase stepping's
  "X" and "Y" are the two motors A and B, not head axes: the motor position is `x + y` and
  `x − y` (`extract_physical_position` in `phase_stepping.hpp`). A calibration move on one
  motor is a diagonal head move; a head move along X or Y runs both motors at the same speed.
  Every speed in this plan is a **motor (belt) speed**, given in rev/s where possible:
  400-step motors, 100 electrical periods per revolution, 32 mm per revolution with 2GT belts
  (0.32 mm per electrical period; the fork's 1.5GT option gives 0.315 mm).
- `CorrectedCurrentLut` adds `Σ mₙ·sin(nθ + φₙ)` (harmonics 1–16, radians of electrical
  angle) to the commanded electrical angle θ; coil currents are `248·sin/cos` of the result.
  One table per motor and per direction of travel; nothing depends on speed
  (`lib/Marlin/Marlin/src/feature/phase_stepping/lut.cpp`, `phase_stepping.cpp`). During
  calibration one extra harmonic is computed live in the ISR with `sin_lut`
  (`_phase_shift_for_harmonic`, `calibration_hooks.hpp`).
- Calibration (`calibration.cpp`, `calibration_config.hpp`) enables only harmonics 2 and 4
  (`0b1010`). A speed sweep (0.2–4 rev/s) finds, per harmonic, the speed with the largest
  measured response (usually the frame resonance, ≈200 Hz, estimate from Prusa's old hand-set
  speeds); each harmonic is then
  tuned at 0.9× that speed only: doubling magnitude search → 4 phase-sweep moves → 4
  magnitude-sweep moves.
- The reported score comes from the same sweep that picked the value (`*max_it / *baseline_it`),
  so it is always ≤ 1 and `makes_improvement()` is always true; nothing re-measures the final
  correction. One failing harmonic fails the whole axis.
- Results live in `/system/phase_step_{x,y}_{fwd,bck}` as `CorrectionSaveFormat`
  (32 reserved zero bytes + 17 × {mag, pha} floats). `M971 … I M P` writes a harmonic and
  **saves the file immediately**; it refuses while phase stepping is enabled (`M97x.cpp`).
  Reading needs the flags: `M971 X Y F B` (plain `M971` prints nothing). `M971 … R` only
  deletes the files; the table in RAM stays until the next load. `M972 X|Y` runs the whole
  calibration over USB, prints per-phase scores and saves the result.

## Hardware budget (INDX)

| Part | Fact | Consequence for this plan |
|---|---|---|
| Main MCU | STM32F42x, Cortex-M4F, 168 MHz (`core_init.cpp`: HSE 12 MHz, PLLM 6, PLLN 168, PLLP 2) | Float fitting during calibration is cheap; the 40 kHz ISR is the tight part |
| Flash | 1 328 844 B of 1919 KB used (67.6 %) in the `coreone_indx` build of 2026-11-12 | ~600 KB free: code size is no constraint |
| Static RAM | 123 700 B of 196 508 B (63 %); CCMRAM 94.5 % full | New runtime tables go to normal RAM; never CCMRAM |
| Heap | newlib `malloc` on the RAM left after static data and stacks (`src/common/heap.cpp`); out of memory = BSOD (a failed contiguous allocation); the `heap` metric (`app_metrics.cpp`) is **total** free bytes, sampled at most every 503 ms: not a low-water mark, blind to fragmentation | Calibration sample buffers (`SignalContainer`, 2 B per sample, 512-sample segments) are the memory risk; free heap during calibration is **unmeasured**, and the stock metric cannot measure it (Phase 0 item 3 samples it) |
| Phase stepping ISR | 40 kHz, one motor per ISR, so each motor is updated at 20 kHz; source notes "every microsecond saves about 4 % of CPU load" | Runtime addition budget ≤ 0.25 µs per ISR (≤ 1 % CPU, ≈ 42 cycles). At 2.5 kHz (harmonic 4 at 200 mm/s motor speed) only 8 updates fall in one period |
| Driver | TMC2130 in XDIRECT, signed 9-bit coil currents; full-scale tolerance ±5 % (datasheet rev 1.16 p. 92); no offset spec, no offset calibration; zero-crossing "ledge" with too little hysteresis (p. 49) | Harmonic 1 from the driver is not expected; coil mismatch (harmonic 2) and zero-crossing error (harmonic 4 family) are |
| Correction resolution | int8 phase-shift table in 1/4096 of an electrical period; simulation of the `lut.cpp` arithmetic (scratch script, not kept; numbers recorded here): corrections ≥ 0.002 rad realized at 92–101 %, < 0.0008 rad lost; a 0.001 rad correction adds 0.8× its size as broadband rounding noise. Range ±127/4096 period = **±0.195 rad**; stock code does not clamp the sum (`lut.cpp`), while the calibration search may go up to `max_magnitude = 0.4` rad and beyond (int8 wrap, unverified whether real values get near it) | Do not store corrections < 0.002 rad; saturate the summed correction at ±0.195 rad |
| Accelerometer | AIS2IH on the INDX head at 1600 Hz, its maximum ODR (ST); HP noise 90 µg/√Hz; the head packs samples to 10 bit; range ±2 g is the chip default (not read from code); its digital low-pass (CTRL6 `BW_FILT`) is never set by the driver, so the default applies (attenuation near 640 Hz not checked) | Band ≤ 800 Hz. With a 20 % margin (≤ 640 Hz): harmonic 4 observable up to 1.6 rev/s (≈ 51 mm/s motor speed), harmonic 2 up to 3.2 rev/s (≈ 102 mm/s). **Aliasing:** exactly at those edges harmonic 6 (resp. 3) folds onto the target (960 → 640 Hz); every sweep speed is checked for `|m·f_e mod f_s|` (m ≤ 16, measured f_s) near the target (Phase 2) |
| Head link | RS485 230 400 Bd shared puppy bus; head MCU STM32C0 at 48 MHz | No on-head processing; no higher sample rate possible |
| Microphone | none on the printer | Sound at print speed (0.6–2.5 kHz tones) cannot be measured by the printer; it must be inferred from a speed model and validated externally |
| Storage | 32 reserved bytes in each correction file | Room for a version tag; new data goes to new files so stock firmware keeps reading the old ones |

INDX versus the plain Core One: Prusa's developer described the Core One calibration (with the
attachable accelerometer, before INDX) as one that "fails too often" because of "BSOD caused
by OOM" and as "on the edge of the hardware capabilities" (issue #4461). INDX has the same
main MCU but a different measurement path: the accelerometer is built into the head, samples
at 1600 Hz instead of the plain Core One's LIS2DH12 at 1344 Hz (≈ 19 % more samples per
second, `4aaf8dbf8` enlarged the buffers for it) and arrives over RS485. Since that comment
Prusa also reduced the calibration's memory and stack use (`cdff11a73`, `6acc03d29`). Whether
INDX calibration still runs close to the memory limit is unmeasured; Phase 0 item 3 records it.

### Compared with Bambu

What is documented (research 2026-11-24):

- **Mechanism:** Bambu publishes only the effect. Its wiki says the compensation is
  calculated "in real time based on the motor's current operating state" (A1 mini UI
  explanation). That sentence fits a speed-dependent model but also a fixed table indexed by
  motor angle like Prusa's; **speed dependence at Bambu is inferred, not documented**.
  Calibration takes about 10–15 min and must be rerun after firmware updates (X1E release
  history). The sensor is never named; an arXiv survey calls it "adaptive current control"
  but found no patent or engineering statement. A title search of Google Patents for the
  assignee found no motor-noise patent (not exhaustive).
- **Hardware:** driver ICs, motion MCU and motor step angle are **not verified** for any
  model; no readable teardown names them. The H2D uses steppers on X/Y (a servo only in the
  extruder).
- **Conclusion:** we cannot claim Bambu has better sensing hardware, and this plan does not
  copy a known Bambu method. Speed-dependent use of the data is our own hypothesis, tested in
  the calibration's own self-gate (Phase 2).

What Prusa says about its own limits:

- Core One calibration (pre-INDX): see the quote above (issue #4461); the same developer
  says phase stepping "just corrects the non-linearity of the motors".
- Repeated XL runs gave inconsistent scores (issues #4726, #4813).
- The XL blog: works at certain speeds in constant-speed moves; the percentage measures
  vibration, not the noise itself; results depend on movement direction.

Prior art: US 7 733 051 B2 injects compensating current for 2nd and 4th harmonic detent
torque (fixed speed, per search summary); Arias et al., IEEE TIE 2018, use a speed-adaptive
resonant controller against cogging in hybrid steppers (closed loop, needs a position sensor
we do not have). Neither was read in full.

## Physics the plan relies on (model; the calibration tests it itself)

Two-phase torque, rotor lagging the commanded angle by the load angle δ:
`T ∝ −i_a·sin θ_r + i_b·cos θ_r`, `θ_r = θ − δ`. A correction Δ added to the commanded angle
changes torque by `Δ·cos δ`.

- **Coil mismatch** (`i_a` scaled by 1+ε) gives a ripple `∝ (ε/2)·sin(2θ − δ)`; the best
  correction is `(ε/2)·sin(2θ − δ)/cos δ`: its phase moves by **one** δ, its size grows by
  1/cos δ.
- **Current offset** in one coil gives `∝ sin(θ − δ)`: also **one** δ.
- **Rotor-tied error** (detent torque, tooth shape) at harmonic n gives `∝ sin(n(θ − δ))`:
  its phase moves by **n·δ**.
- **Zero-crossing error** of the driver is the same odd distortion in both coils, so it adds
  3rd and 5th *current* harmonics (no 4th). With `i_a = I cos θ`, `i_b = I sin θ`, the 3rd
  gives torque `∝ −c₃·sin(4θ − δ)` and the 5th `∝ c₅·sin(4θ + δ)`. The resulting harmonic-4
  ripple therefore moves by anything between −δ and +δ depending on c₃/c₅, and may even look
  speed-independent; its size grows with dI/dt, i.e. with speed. Which mechanism dominates
  harmonic 4 on our motors is unknown; fit a size slope for harmonic 4 by default.
- δ grows with the torque the motor delivers: with speed (friction, back-EMF, driver lag)
  **and with acceleration**. Open model question: is speed alone a good enough index?
- Forward and backward tables already differ because δ flips sign with direction; for each
  harmonic, `φ_fwd − φ_bck ≈ 2·kₙ·δ(v₀)` with kₙ = 1 or n (or between −1 and +1 for the
  zero-crossing case). This does **not** settle kₙ from existing data: harmonic 2 may itself
  be rotor-tied (k = 2), the two harmonics are calibrated at different speeds (different δ),
  phases wrap modulo 2π (4·2δ wraps once δ > 45° electrical), and other direction-dependent
  effects (driver current lag, zero-crossing hysteresis) enter the difference too. kₙ comes
  from same-speed comparisons unwrapped across speeds (Phase 2 sweep).
- The ISR knows the **planned** motor speed (`physical_speed`), not the achieved one; with
  input shaping the real speed oscillates around it. Above 6 rev/s the firmware lowers the
  motor current (`regulate_current<6.f, 10.f, 150>`), which changes δ in a way a fit from
  < 3 rev/s data cannot see.
- A phase error ψ leaves `r = 2·sin(ψ/2)` of the ripple; a correction does harm once ψ > 60°.

Consequence: do **not** assume one lag shared by all harmonics. Fit each calibrated
harmonic's phase as a function of speed, `φₙ(v) = φₙ(v₀) − sₙ·(v − v₀)` (plus a quadratic term
if the data need it), and optionally its size.

## Approved plan

Owner, 2026-11-24: no USB link to the printer is possible; develop the feature so it works.
So there is no separate measuring phase over USB. The firmware measures itself, applies a
speed model only where its own data support it, and otherwise behaves exactly like stock.
Every number leaves the printer over PrusaLink (or on the screen); test moves run as G-code
files from the USB stick.

### Phase 0 — Instruments (no change to printing behaviour)

1. **PrusaLink dump**, read-only `GET /api/v1/phase-stepping` (pattern:
   `filament_renderer.*`, routed in `prusa_link_api_v1.cpp`): per motor and direction the
   stored correction (17 × size, phase), the last calibration record (Phase 1–2 values), the
   heap minimum of the last calibration, ISR timing, missed-transmission count and whether
   the speed model is active.
2. **ISR timing:** DWT cycle counter around the phase stepping refresh, kept as maximum and
   running mean. Gives the before/after ISR cost without a debugger. Its own cost is ≈ 20–25
   cycles per ISR (≈ 0.5 % CPU; counted from the built code, `04bad5639`); exception entry
   and exit are outside the measured window.
3. **Heap during calibration:** sample `xPortGetFreeHeapSize()` and `mallinfo().ordblks` at
   the points where all sample buffers of a move are alive (speed sweep and parameter sweep
   in `calibration.cpp`); keep the minimum in the calibration record. Known gap: the
   short-lived DFT buffers inside the sliding-window helpers (12 B per window sample) are
   freed before any sample point, so the true low point is up to that much lower. Newlib has no
   largest-free-chunk value, and `_sbrk_r` never sees `free()`, so it is not the hook.
4. **`M975 S0|S1`:** speed model off/on at runtime (not persisted; on after boot when a
   model exists), usable from a G-code file. `M975` without `S` reports the state.

Design rule from now on: the new calibration allocates nothing beyond what a stock move
allocates (stock move lengths; repeats are averaged by result, not by samples).

### Phase 1 — Verified calibration (small, low risk)

- After each harmonic: one verification move at the calibration speed with the new
  correction and one with it zeroed; score = residual ratio of these two moves. (The stock
  score is best versus worst point of the magnitude sweep, not best versus zero; its index 0
  is the zero-correction point and is a cheaper first check.) The pair runs twice; keep the
  correction only if both ratios are below 0.7 (to be retuned from the recorded spreads);
  otherwise store zero and report it.
- A harmonic that fails after retries is stored as zero and reported; the axis no longer fails.
- Drop any size < 0.002 rad (cannot be realized cleanly by the int8 table); saturate the
  summed table peak at ±0.195 rad instead of letting int8 wrap, with a unit test for the
  wrap. Keep the stock magnitude search below π/8 = 0.393 rad, where the fixed-point
  product of `_phase_shift_for_harmonic` would overflow int32.
- Reporting: the wizard hook `on_calibration_phase_result` takes two floats only; the
  per-harmonic verification result gets its own hook and screen line.
- Store each harmonic's verification ratios in the new versioned file (see Phase 2 Data), so
  the dump and Phase 3 show them without a format change.

### Phase 2 — Speed-aware correction ("use the data"), self-gating

Calibration:
- Keep the stock speed sweep and the tuning at the resonance speed (best signal).
- Add a **phase-versus-speed sweep** per calibrated harmonic and direction over the
  observable band (harmonic 4 up to 1.6 rev/s, harmonic 2 up to 3.2 rev/s), at 3 speeds per
  harmonic (harmonic 2: 1, 2, 3 rev/s; harmonic 4: 0.5, 1.0, 1.5 rev/s), each twice. Speeds
  are chosen at run time from the measured `sampling_freq` so that no harmonic m ≤ 16 folds
  within 30 Hz of the target (`|m·f_e mod f_s|`, folded); a default that fails is moved by up
  to ±0.2 rev/s. Every move, ramps and settling included, stays within the stock
  `max_movement_revs` (5 revs): the constant-speed part is shortened at high speed, with a
  floor of 2 revs (20 analysis windows); a speed that cannot meet it is skipped.
- Speed models use |v| per direction (`physical_speed` is signed). Calibration runs with all
  speed models off.
- Fit per motor, direction and harmonic: slope sₙ with its standard error σₛ; a size slope
  for harmonic 4 only, from one magnitude search at the top speed.
- **Self-gate:** apply the model for a harmonic only if (a) the phase change it predicts
  across the band exceeds twice the spread of the repeated measurements, and (b) a
  verification move at the top band speed with the model beats the stock table at that
  speed. Otherwise sₙ = 0 and that harmonic behaves exactly like stock.
- Expected total time ≈ 11.6 min for both motors (stock ≈ 3 + verification ≈ 1.5 + speed
  model ≈ 7.2; estimate, not measured; Owner accepted 8–12 min).

Data:
- New file per motor `/system/phase_step_{x,y}_speed` with a version tag, v₀, sₙ, σₛ, size
  slope per harmonic and direction, the verification ratios, and the calibration heap
  minimum. The stock `_fwd`/`_bck` files keep their exact layout, so stock firmware still
  loads them and ignores the new file.

Runtime (ISR):
- **Tables (default):** one int8 table per speed-modelled harmonic and direction, stored
  doubled (2048 B) so the ISR reads it without index wrapping; allocated on the heap with
  `malloc_fallible` only while a model is active (at most 2 harmonics × 2 directions × 2
  motors = 16 KB). If an allocation fails, that harmonic stays in the summed table (stock
  behaviour). The stock summed
  table is rebuilt **without** those harmonics, so none is applied twice. Per update:
  `shift = base[idx] + Σₕ gₕ(v)·tblₕ[(idx − offₕ(v)) & 1023]`, with
  `offₕ(v) = sₕ·(|v| − v₀)·1024 / (2π·h)` (sign and scale checked) and gₕ the size factor
  (1 when no size slope), saturated to int8. offₕ and gₕ change slowly: recompute them only
  when |v| changes by more than a step, not on every update.
- Live `sin_lut` per harmonic is rejected as default: the stock current lookup already
  costs ≈ 800 ns per update (source comment), so two more `sin_lut` calls would exceed the
  budget about threefold.
- Extrapolation above the band edge v_e: continue with slope c·sₙ, c = clamp(1 − 2σₛ/|sₙ|,
  0, 1), hold the value from 6 rev/s up (current reduction starts there), and clamp the
  shift to 1.5× the largest measured.
- Budget: the Phase 0 ISR timing before and after; at most +0.25 µs per ISR (42 cycles at
  168 MHz, 1 % CPU at 40 kHz). Built result (`abd5733b9`, counted from the ELF, not
  measured): model off ≈ 5 cycles (load, compare, branch); one active harmonic ≈ 37 cycles,
  two ≈ 38–42; recompute out of line only when |v| moves by more than 0.05 rev/s. The dump's
  ISR timing shows the real cost on the printer.

Fallback if the phase does not follow a smooth curve: 3 speed-binned tables per harmonic and
direction, blended linearly between neighbouring bins (same file versioning).

### Phase 3 — Show and reuse the data

- A small PrusaLink web page plotting the dump (size, phase, verification, sₙ per speed).
- A 1-minute **check** mode: re-measure the residual of harmonics 2 and 4 at their
  calibration speeds without recalibrating, and suggest recalibration when the residual has
  doubled since the last calibration (belt tension change, motor swap).

### Phase 4 — Validation (by ear, no USB)

- A G-code file on the USB stick (`doc/` gets the file and its instructions): constant-speed
  moves at 50, 100, 150, 200 mm/s motor speed, first `M975 S0`, then `M975 S1`. Diagonal
  head moves (+45°: only motor A runs; −45°: only motor B; motor speed = √2 × head speed, so
  200 mm/s motor speed is F8485) and X and Y head moves (both motors at head speed, F12000
  for 200 mm/s).
- Owner listens; a phone recording is optional (same position, 3 runs, median of the motor
  tone peaks at n·f_e).
- VFA test print (vertical fine artifacts) at 100 and 200 mm/s outer walls (head speed),
  stock vs new.
- Build `--preset coreone_indx` with `-Werror` (Docker, see `Firmware-Build-Instructions-Docker.md`).

What only the printer can prove (not provable in this repo): the benefit at print speed,
which lies above the accelerometer band.

## Built (2026-11-24)

Commits on `v8.0.1-Argo-stable`: gate `01e3af829`, `f04fffe0a`; diagnostics `2396ef314`;
instruments `04bad5639`; dump `e154266bc`; page `9d550d703`; sound test `98982486e`;
verified calibration `5d5a66e32`; speed model `abd5733b9`; check mode `M976` `2f86c819e`;
docs `9811e8c1c`; review fixes `1d0e291f6`; test files home like the calibration and the
dump shows free heap (close round, `b8bb358d5`). Two fresh-eyes verifications passed (interim and
final, plus a re-check of the fixes). Unit tests and the `coreone_indx -Werror` build are
green; nothing has run on a printer yet.

Follow-ups: the page's `?src=` stays on the printer after a security review (`385b3e507`);
nhttp unit tests build again, with exact-output tests for the phase stepping renderer
(`f909ad6f9`); tests for the record file (v1, v2, broken and missing files) and for the
runtime tables (apply, deactivate, allocation failure) (`6fa671794`). Still untested: the
record-file reject paths for a wrong header or record size, record count or harmonic index;
the heap reset on a bad file; the renderer with an invalid check result.

## Answers (Owner, 2026-11-24)

1. A calibration of ≈ 8–12 min instead of ≈ 3 is acceptable.
2. No USB connection to the printer is possible; measurements must come out another way.
3. Develop the feature so it works (Phase 3 is the last part, after the working feature).

## Acceptance checks

- [x] Unit tests (host, Docker): saturation instead of int8 wrap; the speed fit recovers a
      known sₙ from synthetic phases; the self-gate keeps sₙ = 0 on flat data; stock file
      layout unchanged; no harmonic applied twice. (7218 assertions green, 2026-11-24)
- [x] `coreone_indx` builds green with `-Werror`. (2026-11-24)
- [ ] Phase 1: verification ratios shown on screen and in the dump; a failing harmonic no
      longer fails the axis.
- [ ] Phase 2: ISR time increase ≤ 0.25 µs per ISR (dump, DWT).
- [ ] Enough heap on the printer: static RAM grew by ≈ 4.6 KB and the speed model keeps up
      to 16 KB while on, so the stock comparison cannot hold. Instead: the calibration heap
      minimum (dump) stays positive with margin, and the dump's free heap with `M975 S1`
      leaves room for a long print (judge against the value with `M975 S0`).
- [ ] With `M975 S0` the correction equals stock exactly (same table bytes).
- [ ] By ear / recording: motor tones at 100–200 mm/s motor speed lower with `M975 S1` than
      with `S0`, per motor and on X/Y moves; with a recording at least 3 dB on the median of
      3 runs; no audible regression at 50 mm/s.
- [ ] No VFA regression on the test print: X, Y and diagonal walls, photographed under
      raking light, compared blind against the stock print.
- [ ] Stock correction files still load in stock firmware after an Argo calibration.

## Risks

- `calibration.cpp` is upstream code that Prusa keeps changing; keep Argo changes in new files
  (`calibration_speed.cpp`, a `lut` extension) with small hooks, to keep rebases cheap. The
  runtime hook sits in `refresh_axis`/`CorrectedCurrentLut`, which are one translation unit
  (`phase_stepping.cpp` includes `lut.cpp`); a `lut` extension must be included the same way
  to stay inlined.
- The ISR follows the planned speed, not the achieved one (input shaping makes them differ),
  and above 6 rev/s the motor current is reduced, changing δ outside the measured range.
- Stock code lets the int8 correction table wrap above ±0.195 rad; may be a latent upstream
  bug (report to Prusa if the dump shows real values near it).
- The speed model is unverified; the self-gate falls back to stock per harmonic, and the
  dump shows whether speed bins are needed instead of the slope fit.
- Acceleration also changes the load angle; a speed-only model may be wrong during
  acceleration phases.
- Extrapolation above 1.6/3.2 rev/s (≈ 51/102 mm/s motor speed) cannot be measured by the
  printer; only the by-ear validation in Phase 4 shows whether it helps.
- Out-of-memory BSOD during calibration (Prusa's pre-INDX failure mode); mitigated by keeping
  move length and averaging results instead of samples.
- Calibration noise: neighbouring printers or fans disturb the longer off-resonance runs
  (Prusa's help article warns about neighbouring printers).

## Sources

- TMC2130 datasheet rev 1.16, Analog Devices (pp. 47, 49, 90–92):
  https://www.analog.com/media/en/technical-documentation/data-sheets/TMC2130_datasheet_rev1.16.pdf
- AIS2IH, STMicroelectronics (ODR 1.6–1600 Hz, ±2–16 g): https://www.st.com/en/mems-and-sensors/ais2ih.html
- Bambu Lab wiki, A1 mini UI explanation (motor noise cancellation):
  https://wiki.bambulab.com/en/a1-mini/manual/ui-explanation
- Bambu X1E firmware release history:
  https://wiki.bambulab.com/en/x1/manual/X1E-firmware-release-history
- arXiv survey mentioning Bambu's "adaptive current control": https://arxiv.org/html/2606.13952
- Prusa blog, Phase Stepping on the XL:
  https://blog.prusa3d.com/phase-stepping-how-we-busted-vibrations-and-improved-print-quality-on-the-xl-printer-with-just-a-firmware-update_94793
- Prusa-Firmware-Buddy issues #4461, #4813:
  https://github.com/prusa3d/Prusa-Firmware-Buddy/issues/4461 ,
  https://github.com/prusa3d/Prusa-Firmware-Buddy/issues/4813
- University of Iowa, Microstepping (4th harmonic from detent torque):
  https://homepage.divms.uiowa.edu/~jones/step/micro.html
- US 7 733 051 B2: https://patents.google.com/patent/US7733051
- Arias et al. 2018 (speed-adaptive cogging compensation):
  https://www.cd6.upc.edu/content/cd6_upload/contribucions/2019012316502316116536165c488d3f08e86.pdf
