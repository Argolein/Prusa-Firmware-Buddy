# INDX speed-reactive status LED

Status: implemented & built (branch `v6.6.0-indx-rgb`, based on stock `v6.6.0`).
`coreone_indx_release_boot` builds clean with `-Werror`. Default on; toggle in
Settings -> User interface -> Lights Settings -> "Speed Reactive Light".

## Objective

Make the INDX tool-board RGB status LED (driven by the on-head LP5817 over I2C)
change color with the current motion speed, as a "fun" feature:

- Flat **green** up to ~90 mm/s.
- Smooth gradient **green → yellow → orange → bright red** up to 300 mm/s.

The LED is a single full-RGB LED (LP5817, 8-bit per channel), so the gradient is
continuous, not a fixed set of colors.

## Hard constraint: no measurable CPU load

The feature must not add a hot path or a new thread/timer, and must not flood the
RS-485 link to the head. Design choices that satisfy this:

1. **No new scheduling.** The color is recomputed from the existing
   `_server_update_vars()` periodic tick (`MARLIN_UPDATE_PERIOD = 100 ms`, i.e.
   ~10 Hz) on the Marlin server task.
2. **No extra Modbus traffic at steady state.** `indx.set_leds_color()` only
   marks the puppy write dirty; `Indx::write_general()` already runs in the puppy
   round-robin and `PuppyModbus::write_holding()` skips the transfer when not
   dirty. We only call `set_leds_color()` when the mapped color actually changes,
   so a constant speed (or idle) produces **zero** writes. Speed is taken from
   the executing block's `nominal_speed` (constant within a block), so the color
   is stable mid-move and only steps at move boundaries with a speed change.
3. **No floating-point ramp tables.** `speed_to_color()` is a tiny constexpr
   piecewise-linear interpolation over 9 keyframes (integer math). Per tick:
   2 config reads + 1 planner read + 1 EWMA step + 1 map. Negligible.
4. **Disabled = free.** Two cached config reads then early return.

## Architecture (keeps the puppy layer motion-free)

The puppy library deliberately does not include Marlin motion headers. So the
speed is read on the **Marlin side** and pushed to the head via the existing
public, thread-safe `buddy::puppies::indx.set_leds_color()`:

```
_server_update_vars()  (Marlin task, ~10 Hz)
  -> indx_speed_led::update()
       reads planner block nominal_speed
       EWMA smoothing -> speed_to_color() -> set_leds_color() [only on change]
  -> Indx::write_general()  (puppy task) flushes RGB over Modbus when dirty
       -> LP5817 on the head (gamma 2.2 re-applied on the head)
```

Files:

- `src/common/indx_speed_led.{hpp,cpp}` — mapping (constexpr, unit-testable) +
  `update()` glue. New, self-contained; guarded by `HAS_INDX()`.
- `src/common/marlin_server.cpp` — one `indx_speed_led::update()` call in the
  `#if HAS_INDX()` part of `_server_update_vars()`.
- `src/common/CMakeLists.txt` — add the new source to the firmware target.
- config: `tool_leds_speed_reactive` (bool, default true).
- GUI: `MI_INDX_SPEED_LED` switch in the "Lights Settings" menu
  (`screen_menu_leds.hpp`), guarded by `HAS_INDX()`.

## Color ramp (keyframes, sRGB targets)

| Speed (mm/s) | Color | RGB |
|---|---|---|
| 0 – 90 | green | 21, 194, 75 |
| 120 | lime | 154, 214, 28 |
| 150 | yellow | 232, 208, 0 |
| 180 | amber | 255, 179, 0 |
| 210 | orange | 255, 122, 0 |
| 240 | orange-red | 255, 69, 0 |
| 270 | red | 240, 24, 0 |
| 300 | bright red | 212, 0, 0 |

The head applies gamma 2.2 to these values, so on the physical LED the ramp looks
a touch more saturated/darker than the sRGB targets above — final stops are worth
tuning once on hardware.

## Notes / decisions

- Speed source: executing block `nominal_speed` (cruise speed), 0 when no block
  is queued. Cross-thread plain read; a torn/stale sample is harmless (one frame,
  smoothed by EWMA). Chosen over `feedrate_mm_s` because the block reflects the
  move actually executing rather than the parser look-ahead.
- E-only / retraction moves carry their own feedrate; the LED will reflect it.
  Acceptable for a fun feature; could be filtered later.
- Default on (`tool_leds_speed_reactive = true`): out of the box the INDX LED is
  speed-reactive instead of static brand orange. Toggle off in
  Lights Settings → "Speed Reactive Light" to restore the static behavior.

## Build / validation

INDX-only paths -> build `--preset coreone_indx` with `-Werror`
(per `ARGO-DEVELOPMENT.md`).
