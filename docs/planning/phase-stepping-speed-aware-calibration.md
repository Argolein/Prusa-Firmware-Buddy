# Speed-aware phase stepping calibration (INDX) — plan

> **Status: PROPOSED** (2026-11-24). Not approved, nothing implemented. Scope: Core One INDX
> only (`HAS_INDX()`), branch `v8.0.1-Argo-stable`.

## Objective

Make the X/Y motors measurably quieter at print speed by improving how phase stepping is
calibrated **and** how the calibration data is used while printing:

1. calibrate the correction at several speeds instead of one, and apply it as a function of
   speed (today one table serves every speed);
2. verify every correction with an independent measurement before keeping it;
3. expose the measured data so its quality can be judged and tracked over time.

All of it has to fit the existing INDX hardware: no new sensors, no driver change.

## How stock phase stepping works (v8.0.1)

- `CorrectedCurrentLut` adds `Σ mₙ·sin(nθ + φₙ)` (harmonics 1–16, radians of electrical
  angle) to the commanded electrical angle θ; coil currents are `248·sin/cos` of the result.
  One table per axis and per direction of travel; nothing depends on speed
  (`lib/Marlin/Marlin/src/feature/phase_stepping/lut.cpp`, `phase_stepping.cpp`).
- Calibration (`calibration.cpp`, `calibration_config.hpp`) enables only harmonics 2 and 4
  (`0b1010`). A speed sweep (0.2–4 rev/s) finds the speed where each harmonic excites the
  frame resonance (≈200 Hz, estimate from Prusa's old hand-set speeds); each harmonic is then
  tuned at 0.9× that speed only: doubling magnitude search → 4 phase-sweep moves → 4
  magnitude-sweep moves.
- The reported score comes from the same sweep that picked the value (`*max_it / *baseline_it`),
  so it is always ≤ 1 and `makes_improvement()` is always true; nothing re-measures the final
  correction. One failing harmonic fails the whole axis.
- Results live in `/system/phase_step_{x,y}_{fwd,bck}` as `CorrectionSaveFormat`
  (32 reserved zero bytes + 17 × {mag, pha} floats). `M971 … I M P` writes a harmonic and
  **saves the file immediately**; it refuses while phase stepping is enabled (`M97x.cpp`).

## Hardware budget (INDX)

| Part | Fact | Consequence for this plan |
|---|---|---|
| Main MCU | STM32F42x, Cortex-M4F, 168 MHz (`core_init.cpp`: HSE 12 MHz, PLLM 6, PLLN 168, PLLP 2) | Float fitting during calibration is cheap; the 40 kHz ISR is the tight part |
| Flash | 1 328 844 B of 1919 KB used (67.6 %) in the `coreone_indx` build of 2026-11-12 | ~600 KB free: code size is no constraint |
| Static RAM | 123 700 B of 196 508 B (63 %); CCMRAM 94.5 % full | New runtime tables go to normal RAM; never CCMRAM |
| Heap | newlib `malloc` on the RAM left after static data and stacks (`src/common/heap.cpp`); out of memory = BSOD; free heap is already logged as the `heap` metric (`app_metrics.cpp`) | Calibration sample buffers (`SignalContainer`, 2 B per sample) are the memory risk; free heap during calibration is **unmeasured** |
| Phase stepping ISR | 40 kHz refresh (25 µs period); source notes "every microsecond saves about 4 % of CPU load" | Runtime addition budget ≤ 0.25 µs per axis update (≤ 1 % CPU) |
| Driver | TMC2130 in XDIRECT, signed 9-bit coil currents; full-scale tolerance ±5 % (datasheet rev 1.16 p. 92); no offset spec, no offset calibration; zero-crossing "ledge" with too little hysteresis (p. 49) | Harmonic 1 from the driver is not expected; coil mismatch (harmonic 2) and zero-crossing error (harmonic 4 family) are |
| Correction resolution | int8 phase-shift table in 1/4096 of an electrical period; simulation of the `lut.cpp` arithmetic (scratch script, not kept; numbers recorded here): corrections ≥ 0.002 rad realized at 92–101 %, < 0.0008 rad lost; a 0.001 rad correction adds 0.8× its size as broadband rounding noise | Do not store corrections < 0.002 rad |
| Accelerometer | AIS2IH on the INDX head at 1600 Hz, its maximum ODR (ST); HP noise 90 µg/√Hz; the head packs samples to 10 bit; range ±2 g is the chip default (not read from code) | Band ≤ 800 Hz. With 0.32 mm per electrical period and a 20 % margin (≤ 640 Hz): harmonic 4 observable up to ≈ 51 mm/s motor speed, harmonic 2 up to ≈ 102 mm/s |
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
- **Zero-crossing error** of the driver is a current error at harmonic 4: in this model it
  moves by one δ, not four. Which mechanism dominates harmonic 4 on our motors is unknown.
- δ grows with the torque the motor delivers: with speed (friction, back-EMF, driver lag)
  **and with acceleration**. Open model question: is speed alone a good enough index?
- Forward and backward tables already differ because δ flips sign with direction; for each
  harmonic, `φ_fwd − φ_bck ≈ 2·kₙ·δ(v₀)` with kₙ = 1 or n. This gives a first estimate of kₙ
  from existing data.
- A phase error ψ leaves `r = 2·sin(ψ/2)` of the ripple; a correction does harm once ψ > 60°.

Consequence: do **not** assume one lag shared by all harmonics. Fit each calibrated
harmonic's phase as a function of speed, `φₙ(v) = φₙ(v₀) − sₙ·(v − v₀)` (plus a quadratic term
if the data need it), and optionally its size.

## Approved plan

*(none yet — the phases below are the proposal)*

### Phase 0 — Measure before building (no behaviour change)

Gate for everything else, cheapest first.

1. **Read the existing calibration** (`M971` over USB, one minute): forward-minus-backward
   phase for harmonics 2 and 4 on both axes. Ratio of the two differences gives a first hint
   whether harmonic 4 behaves like a current error (k = 1) or rotor-tied (k = 4).
2. **Check the tools run on INDX:** `utils/phase_stepping/phase_stepping.py` defaults to
   200-step motors (`--motor-steps 400` needed) and sizes buffers with
   `ACCEL_MAX_SAMPLING_RATE = 1300` (INDX samples at 1600 Hz); `M973` must work with the
   remote head accelerometer. Fix the host tool if needed (host side only).
3. **Memory headroom:** log the `heap` metric during a stock calibration; record the minimum
   free heap. Decides whether longer moves are possible or must be split (Phase 2).
4. **Repeatability:** stock wizard 3× per axis with a power cycle in between, `M971` after
   each; spread of size and phase per harmonic.
5. **Speed dependence:** best phase for harmonic 2 at 1, 1.5, 2.5, 3 rev/s and for harmonic 4
   at 0.5, 0.8, 1.2, 1.5 rev/s, both directions; fit sₙ. Off resonance the signal is weaker:
   repeat each stock-length move 2–4× and average the results (not the samples), which needs
   no extra memory.
6. **Harmonic inventory:** size of harmonics 1, 3, 6, 8 with the phase-sweep ratio method
   (`R = (a+m)/|a−m|` at a known test size m, two m values to resolve the ambiguity).
7. **Sound baseline:** phone microphone spectrum of constant-speed X and Y moves at 50, 100,
   150, 200 mm/s, with stock corrections and with corrections zeroed. Zeroing: back up with
   `M971`, `M970` off, `M971` write zeros (this **overwrites the stored files**), `M970` on,
   record, then restore the backup the same way.

Exit: results recorded in this file; go/no-go per phase. Stop after Phase 1 if the phases do
not move measurably with speed (sₙ within the repeatability spread) or the microphone shows no
motor tones above the noise floor.

### Phase 1 — Verified calibration (small, low risk)

- After each harmonic: one verification move at the calibration speed with the new
  correction and one with it zeroed; score = residual ratio of these two moves. Keep the
  correction only if the residual is ≤ 0.7; otherwise store zero and report it.
- A harmonic that fails after retries is stored as zero and reported; the axis no longer fails.
- Drop any size < 0.002 rad (cannot be realized cleanly by the int8 table).
- Extra harmonics (1, 3, 8) only for those Phase 0 found ≥ 0.005 rad and repeatable across a
  power cycle.

### Phase 2 — Speed-aware correction ("use the data")

Calibration:
- Keep the stock speed sweep and the tuning at the resonance speed (best signal).
- Add a **phase-versus-speed sweep** per calibrated harmonic and direction over the
  observable band (harmonic 4 up to ≈ 51 mm/s, harmonic 2 up to ≈ 102 mm/s), using repeated
  stock-length moves averaged by result so the sample buffer does not grow (Phase 0 step 3
  decides whether longer single moves are affordable instead).
- Fit sₙ (and optionally a size slope) per axis, direction and harmonic.
- Expected total time ≈ 8–12 min for both axes (stock ≈ 3 min, estimate).

Data:
- New file per axis `/system/phase_step_{x,y}_speed` with a version tag, v₀, sₙ (and
  quadratic terms) per harmonic and direction, and a fit-quality value. The stock
  `_fwd`/`_bck` files keep their exact layout, so stock firmware still loads them and
  ignores the new file.

Runtime (ISR):
- One int8 table **per calibrated harmonic** and direction (1024 B each; 2 harmonics × 2
  directions × 2 axes = 8 KB, normal RAM) instead of one summed table.
- Per update: `shift = Σₕ tblₕ[(idx − offₕ(v)) & 1023]`, where
  `offₕ(v) = sₕ·(v − v₀)·1024 / (2π·h)` converts the harmonic's phase shift into a table index
  shift, computed from the speed the ISR already has (`physical_speed`). For two harmonics
  this is two lookups and two multiply-adds.
- Above the observable band the fit is extrapolated: clamp the phase shift to 1.5× the largest
  measured, and fade a harmonic to zero above a configurable speed if Phase 4 shows the
  extrapolation hurts.
- Budget check: measure ISR time with the DWT cycle counter before and after; must stay within
  +0.25 µs per axis update.

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
   least step 1 (one `M971` readout). Which?
3. Phase 3 (PrusaLink page, check mode): wanted, or stop after Phase 2?

## Acceptance checks

- [ ] Phase 0 results recorded here, go/no-go per phase decided.
- [ ] Phase 1: verification score reported on screen; a failing harmonic no longer fails the axis.
- [ ] Phase 2: motor tones at 100–200 mm/s at least 3 dB lower than stock calibration on the
      microphone spectrum, both axes, no regression at 50 mm/s.
- [ ] Phase 2: ISR time increase ≤ 0.25 µs per axis update (DWT measurement).
- [ ] Minimum free heap during the new calibration not lower than during stock calibration.
- [ ] No VFA regression on the test print.
- [ ] `coreone_indx` builds green with `-Werror`.
- [ ] Stock correction files still load in stock firmware after an Argo calibration.

## Risks

- `calibration.cpp` is upstream code that Prusa keeps changing; keep Argo changes in new files
  (`calibration_speed.cpp`, a `lut` extension) with small hooks, to keep rebases cheap.
- The speed model is unverified; Phase 0 decides between the slope fit and speed bins, or
  stopping.
- Acceleration also changes the load angle; a speed-only model may be wrong during
  acceleration phases.
- Extrapolation above ≈ 51/102 mm/s cannot be measured by the printer; only the microphone
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
