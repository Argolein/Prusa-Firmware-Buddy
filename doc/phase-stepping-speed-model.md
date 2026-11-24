# Phase stepping speed model (Core One INDX)

The speed model is switched with `M975`: `M975 S0` turns it off, `M975 S1`
turns it on, and `M975` alone reports the current state.

## Sound test

### What the file does

`phase-stepping-speed-model-sound-test.gcode` (in this folder) lets you hear
whether the speed model makes the motors quieter. It runs cold: no heating, no
extrusion, no tool change. A run takes about 11 minutes.

1. Homes the printer (`G28`) and lowers the bed to Z 30.
2. Turns the speed model off (`M975 S0`) and runs the test blocks.
3. Turns the speed model on (`M975 S1`) and runs the same blocks again.
4. Leaves the speed model on, parks the head in the middle of the bed and turns
   the motors off.

Each half has one block per motor speed: 50, 100, 150 and 200 mm/s. Every block
has four move types, each driven back and forth 3 times at constant speed, with
a short pause between move types:

| Move type | Motors that run |
|---|---|
| +45° diagonal (X and Y grow together) | motor A only |
| −45° diagonal (X grows, Y shrinks) | motor B only |
| X only | both motors |
| Y only | both motors |

The screen shows the current step, for example `OFF 100mm/s +45` or
`ON 200mm/s X`.

All moves stay inside X 30–230 and Y 30–190, clear of the tool docks (low Y)
and of the nozzle cleaner (high X). The limits and their sources are
listed in the header of the G-code file.

### Before you start

- **Stealth mode off.** In stealth mode the INDX limits the head to 140 mm/s,
  so the 150 and 200 mm/s X and Y moves would run slower than the file says.
- **Phase stepping on and calibrated** for both motors. Without it both halves
  sound the same.
- **Firmware with `M975`.** Firmware without it reports `M975` as an unknown
  command and carries on, so both halves then sound the same.
- The bed and the head must be free to move; no print on the bed.

### How to run it

1. Copy `phase-stepping-speed-model-sound-test.gcode` to the USB stick.
2. Plug the stick into the printer and print the file like any other print.
3. Stand in the same place for the whole run, or put a phone at a fixed spot
   (same position and distance each time) and record.
4. Compare each block of the OFF half with the same block of the ON half:
   same speed, same move type.

For a fairer comparison run the file three times and compare the typical
result, not one run.

### What to listen for

- **Motor tone at constant speed.** Listen to the middle of each move, not the
  start, stop or turnaround. A whine, buzz or rough tone that is weaker in the ON
  half means the speed model helps at that speed.
- **One motor at a time on the diagonals.** The +45° moves let you hear motor A
  alone, the −45° moves motor B alone. A difference on one diagonal but not the
  other points at that motor.
- **X and Y moves** run both motors together and are closest to what you hear
  during a print.
- At 200 mm/s each move holds its top speed for under one second, so listen
  closely or use the recording.

### Feed-rate table

The Core One is CoreXY. On a ±45° diagonal only one motor runs, at √2 × the head
speed. On an X or Y move both motors run at the head speed.

| Motor speed (mm/s) | Diagonal feed rate (F, mm/min) | X / Y feed rate (F, mm/min) |
|---|---|---|
| 50 | 2121 | 3000 |
| 100 | 4243 | 6000 |
| 150 | 6364 | 9000 |
| 200 | 8485 | 12000 |

Diagonal F = motor speed ÷ √2 × 60; X / Y F = motor speed × 60.

## Check

### What the file does

`phase-stepping-speed-model-check.gcode` (in this folder) checks whether the
last phase stepping calibration still fits the printer, without calibrating
again. It runs cold: no heating, no extrusion, no tool change. It homes, then
`M976` measures each motor at the speeds the calibration used, in both
directions, with the corrections as they are now. The measuring takes about
45 seconds for both motors (estimate, not yet timed on the printer); with homing
and parking the file runs a little over a minute.

For every corrected harmonic the check compares the vibration now with the
vibration the calibration measured right after it chose the correction. When
any ratio is above 2 (the vibration has more than doubled, for example after a
belt tension change or a motor swap), it suggests recalibrating.

### How to run it

1. Copy the file to the USB stick and print it from the printer.
2. Read the result on the screen when the measuring ends:
   - `Phase stepping check: OK` — the calibration still fits.
   - `Phase stepping check: recalibration suggested` — run the phase stepping
     calibration again.
   - `Phase stepping check: no baseline, calibrate first` — the calibration on
     the printer was made by firmware that did not store the comparison values
     yet; calibrate once with this firmware.
   - `Phase stepping check failed` — a measuring move failed; run it again.
3. For the details open the phase stepping page in PrusaLink: the status panel
   shows "Last check since boot", and each motor table has a "Now / baseline"
   column with the ratio per harmonic.

The result is kept until the next check or the next restart; it is not saved.
`M976` can also be sent from the printer console.
