# Speed-aware phase stepping calibration (INDX) — plan

> **Status: PROPOSED** (2026-11-24). Not approved, nothing implemented. Scope: Core One INDX
> only (`HAS_INDX()`), branch `v8.0.1-Argo-stable`.

## Decisions

- 2026-11-24: fresh-eyes plan review (16 findings) folded in: CoreXY motor naming, per-axis
  ISR rate, int8 range, aliasing at the band edges, heap measurement, kₙ ambiguity,
  zero-crossing physics, live per-harmonic runtime option.

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
| Heap | newlib `malloc` on the RAM left after static data and stacks (`src/common/heap.cpp`); out of memory = BSOD (a failed contiguous allocation); the `heap` metric (`app_metrics.cpp`) is **total** free bytes, sampled at most every 503 ms: not a low-water mark, blind to fragmentation | Calibration sample buffers (`SignalContainer`, 2 B per sample, 512-sample segments) are the memory risk; free heap during calibration is **unmeasured**, and the stock metric cannot measure it (Phase 0 step 3) |
| Phase stepping ISR | 40 kHz, one motor per ISR, so each motor is updated at 20 kHz; source notes "every microsecond saves about 4 % of CPU load" | Runtime addition budget ≤ 0.25 µs per ISR (≤ 1 % CPU, ≈ 42 cycles). At 2.5 kHz (harmonic 4 at 200 mm/s motor speed) only 8 updates fall in one period |
| Driver | TMC2130 in XDIRECT, signed 9-bit coil currents; full-scale tolerance ±5 % (datasheet rev 1.16 p. 92); no offset spec, no offset calibration; zero-crossing "ledge" with too little hysteresis (p. 49) | Harmonic 1 from the driver is not expected; coil mismatch (harmonic 2) and zero-crossing error (harmonic 4 family) are |
| Correction resolution | int8 phase-shift table in 1/4096 of an electrical period; simulation of the `lut.cpp` arithmetic (scratch script, not kept; numbers recorded here): corrections ≥ 0.002 rad realized at 92–101 %, < 0.0008 rad lost; a 0.001 rad correction adds 0.8× its size as broadband rounding noise. Range ±127/4096 period = **±0.195 rad**; stock code does not clamp the sum (`lut.cpp`), while the calibration search may go up to `max_magnitude = 0.4` rad and beyond (int8 wrap, unverified whether real values get near it) | Do not store corrections < 0.002 rad; saturate the summed correction at ±0.195 rad |
| Accelerometer | AIS2IH on the INDX head at 1600 Hz, its maximum ODR (ST); HP noise 90 µg/√Hz; the head packs samples to 10 bit; range ±2 g is the chip default (not read from code); its digital low-pass (CTRL6 `BW_FILT`) is never set by the driver, so the default applies (attenuation near 640 Hz not checked) | Band ≤ 800 Hz. With a 20 % margin (≤ 640 Hz): harmonic 4 observable up to 1.6 rev/s (≈ 51 mm/s motor speed), harmonic 2 up to 3.2 rev/s (≈ 102 mm/s). **Aliasing:** exactly at those edges harmonic 6 (resp. 3) folds onto the target (960 → 640 Hz); every sweep speed must be checked for `|m·f_e mod 1600|` (m ≤ 8) near the target. The Phase 0 step 5 speeds are clear (checked within ±25 Hz) |
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
INDX calibration still runs close to the memory limit is unmeasured; Phase 0 measures it.

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
  Phase 0.

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

## Physics the plan relies on (model, to be verified in Phase 0)

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
  from same-speed comparisons unwrapped across speeds (Phase 0 step 5).
- The ISR knows the **planned** motor speed (`physical_speed`), not the achieved one; with
  input shaping the real speed oscillates around it. Above 6 rev/s the firmware lowers the
  motor current (`regulate_current<6.f, 10.f, 150>`), which changes δ in a way a fit from
  < 3 rev/s data cannot see.
- A phase error ψ leaves `r = 2·sin(ψ/2)` of the ripple; a correction does harm once ψ > 60°.

Consequence: do **not** assume one lag shared by all harmonics. Fit each calibrated
harmonic's phase as a function of speed, `φₙ(v) = φₙ(v₀) − sₙ·(v − v₀)` (plus a quadratic term
if the data need it), and optionally its size.

## Approved plan

*(none yet — the phases below are the proposal)*

### Phase 0 — Measure before building (no behaviour change)

Gate for everything else, cheapest first.

1. **Read the existing calibration** (`M971 X Y F B` over USB, one minute): size and phase of
   harmonics 2 and 4, both motors, both directions. A sanity read only (does a calibration
   exist, are sizes far from the ±0.195 rad int8 limit); it cannot settle kₙ (see Physics).
2. **Check the tools run on INDX** — host side done 2026-11-24, no code change needed:
   pass `--motor-steps 400` to `analyzeParamSweep`, `findOptimalMagnitude` and
   `analyzeSpeedSweep` (default 200 serves other printers; Core One returns 400 in
   `get_has_400step_xy_motors`). `ACCEL_MAX_SAMPLING_RATE = 1300` is never used; each
   `M973`/`M974` reply carries its own `sampling_freq`. `M973`/`M974` capture through
   `capture_param_sweep_samples`, the wizard's own path, so they use the head accelerometer;
   not yet run on the printer.
3. **Memory headroom:** the stock `heap` metric cannot answer this (total free bytes, every
   503 ms). Add a debug-build low-water mark (minimum of free heap tracked in `_sbrk_r` or
   sampled in the calibration loop) plus `mallinfo()` largest-free-chunk per move, and say how
   it is captured (syslog host or USB log). Run one stock calibration; record the minimum.
   Decides whether longer moves are possible or must be split (Phase 2).
4. **Repeatability:** `M972 X` and `M972 Y` 3× each over USB with a power cycle in between
   (prints per-phase scores, saves), `M971 X Y F B` after each; spread of size and phase per
   harmonic. The spread sets the Phase 1 keep threshold.
5. **Speed dependence:** best phase for harmonic 2 at 1, 1.5, 2.5, 3 rev/s and for harmonic 4
   at 0.5, 0.8, 1.2, 1.5 rev/s (motor speed; all free of aliased harmonics m ≤ 8), both
   directions; fit sₙ and, for harmonic 4, a size slope. Compare forward and backward at the
   same speed to estimate kₙ. Off resonance the signal is weaker: repeat each stock-length
   move 2–4× and average the results (not the samples), which needs no extra memory.
6. **Harmonic inventory:** size of harmonics 1, 3, 6, 8 with the phase-sweep ratio method
   (`R = (a+m)/|a−m|` at a known test size m, two m values to resolve the ambiguity).
7. **Sound baseline:** phone microphone spectrum (same phone position every run, each run 3×)
   of constant-speed moves at 50, 100, 150, 200 mm/s motor speed: **diagonal** head moves
   (only motor A or only motor B runs, so a tone belongs to one motor) and X and Y head moves
   (both motors, as in printing). Measure the peak of the motor tone lines at n·f_e. Record
   with stock corrections and with corrections zeroed. Zeroing: back up with `M971 X Y F B`,
   `M970` off, `M971` write zeros (this **overwrites the stored files**; one write per index,
   each saving the whole file), `M970` on, record, then restore the backup the same way
   (`M971 … R` is no shortcut: it only deletes the files).

Exit: results recorded in this file; go/no-go per phase. Stop after Phase 1 if the phases do
not move measurably with speed (sₙ within the repeatability spread) or the microphone shows no
motor tones above the noise floor.

### Phase 1 — Verified calibration (small, low risk)

- After each harmonic: one verification move at the calibration speed with the new
  correction and one with it zeroed; score = residual ratio of these two moves. (The stock
  score is best versus worst point of the magnitude sweep, not best versus zero; its index 0
  is the zero-correction point and is a cheaper first check.) Keep the correction only if
  the residual is below a threshold set from the Phase 0 repeatability spread (0.7 as a
  placeholder); otherwise store zero and report it.
- A harmonic that fails after retries is stored as zero and reported; the axis no longer fails.
- Drop any size < 0.002 rad (cannot be realized cleanly by the int8 table); saturate the
  summed table at ±0.195 rad instead of letting int8 wrap, with a unit test for the wrap.
- Reporting: the wizard hook `on_calibration_phase_result` takes two floats only; the
  per-harmonic verification result needs its own hook and screen line (to design).
- Extra harmonics (1, 3, 8) only for those Phase 0 found ≥ 0.005 rad and repeatable across a
  power cycle.

### Phase 2 — Speed-aware correction ("use the data")

Calibration:
- Keep the stock speed sweep and the tuning at the resonance speed (best signal).
- Add a **phase-versus-speed sweep** per calibrated harmonic and direction over the
  observable band (harmonic 4 up to 1.6 rev/s, harmonic 2 up to 3.2 rev/s, skipping speeds
  where an aliased harmonic lands near the target), using repeated
  stock-length moves averaged by result so the sample buffer does not grow (Phase 0 step 3
  decides whether longer single moves are affordable instead).
- Fit sₙ (and a size slope, by default for harmonic 4) per motor, direction and harmonic.
- Expected total time ≈ 8–12 min for both motors (stock ≈ 3 min, estimate).

Data:
- New file per motor `/system/phase_step_{x,y}_speed` with a version tag, v₀, sₙ (and
  quadratic terms) per harmonic and direction, and a fit-quality value. The stock
  `_fwd`/`_bck` files keep their exact layout, so stock firmware still loads them and
  ignores the new file.

Runtime (ISR), two candidates; the DWT measurement picks one:
- **A, live (preferred):** keep the stock summed table for harmonics without a speed model and
  compute the speed-modelled harmonics live, as calibration already does
  (`_phase_shift_for_harmonic`): `mag·sin_lut(h·idx·SIN_FRACTION + pha(v))`, summed in int
  before the one final rounding and saturation. No new RAM, phase in 1/4096 steps, no
  per-harmonic rounding noise. Cost: two extra `sin_lut` calls per ISR; may not fit ≈ 42
  cycles.
- **B, tables:** one int8 table per calibrated harmonic and direction (1024 B each;
  2 harmonics × 2 directions × 2 motors = 8 KB, normal RAM). Per update:
  `shift = Σₕ tblₕ[(idx − offₕ(v)) & 1023]` with `offₕ(v) = sₕ·(v − v₀)·1024 / (2π·h)`
  (sign and scale checked). Each table is rounded separately (rounding noise adds per
  harmonic) and the phase step is 1/1024.
- Both use the speed the ISR already has (`physical_speed`, the planned motor speed) and
  `mm_to_rev`.
- Above the observable band the fit is extrapolated: clamp the phase shift to 1.5× the largest
  measured, and fade a harmonic to zero above a configurable speed if Phase 4 shows the
  extrapolation hurts.
- Budget check: measure ISR time with the DWT cycle counter before and after; must stay within
  +0.25 µs per ISR.

Fallback if the phase does not follow a smooth curve: store 3 speed-binned tables per
harmonic and direction and blend linearly between neighbouring bins (same file versioning).

### Phase 3 — Show and reuse the data

- Dump of the new data as `M975` (M970–M974 are taken, M975–M979 unused in `gcode.cpp` as of
  v8.0.1-Argo) and a read-only PrusaLink endpoint `GET /api/v1/phase-stepping` (pattern:
  `filament_renderer.*`) with per-harmonic size, phase, verified reduction and sₙ; a small web
  page plotting it.
- A 1-minute **check** mode: re-measure the residual of harmonics 2 and 4 at their
  calibration speeds without recalibrating, and suggest recalibration when the residual has
  doubled since the last calibration (belt tension change, motor swap).

### Phase 4 — Validation

- Repeat the Phase 0 microphone runs with Phase 1 + 2 firmware.
- VFA test print (vertical fine artifacts) at 100 and 200 mm/s outer walls, stock vs new.
- Build `--preset coreone_indx` with `-Werror` (Docker, see `Firmware-Build-Instructions-Docker.md`).

## Open questions (Owner)

1. Is a calibration of ≈ 8–12 min (instead of ≈ 3) acceptable?
2. Phase 0 needs you at the printer (USB host tool, phone microphone, about 1 hour), or at
   least step 1 (one `M971 X Y F B` readout). Step 3 also needs a debug firmware build with a
   heap low-water mark flashed for one calibration. Which?
3. Phase 3 (PrusaLink page, check mode): wanted, or stop after Phase 2?

## Acceptance checks

- [ ] Phase 0 results recorded here, go/no-go per phase decided.
- [ ] Phase 1: verification score reported on screen; a failing harmonic no longer fails the axis.
- [ ] Phase 2: peak of the motor tone lines at 100–200 mm/s motor speed at least 3 dB lower
      than with stock calibration, per motor (diagonal moves) and on X and Y head moves; same
      phone position, 3 runs each, run-to-run spread below 3 dB; no regression at 50 mm/s.
- [ ] Phase 2: ISR time increase ≤ 0.25 µs per ISR (DWT measurement).
- [ ] Minimum free heap (low-water mark from Phase 0 step 3) during the new calibration not
      lower than during stock calibration.
- [ ] No VFA regression on the test print: X, Y and diagonal walls, photographed under
      raking light, compared blind against the stock print.
- [ ] `coreone_indx` builds green with `-Werror`.
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
  bug (report to Prusa if Phase 0 shows real values near it).
- The speed model is unverified; Phase 0 decides between the slope fit and speed bins, or
  stopping.
- Acceleration also changes the load angle; a speed-only model may be wrong during
  acceleration phases.
- Extrapolation above 1.6/3.2 rev/s (≈ 51/102 mm/s motor speed) cannot be measured by the printer; only the microphone
  validation in Phase 4 shows whether it helps.
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
