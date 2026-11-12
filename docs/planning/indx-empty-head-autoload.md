# INDX empty-head autoload — design notes

> **Status:** implemented on `v6.9.1-Argo-stable` (2026-11-12); not yet exercised on the printer.

## Problem

On INDX, autoload stopped working after the first print or load. Stock firmware docks the
tool at the end of every print (`pre_finalize_print` → `tool_change(NoTool)`), after every
menu load (`M701_load`) and after every autoload (`M1701_autoload`), and persists "no tool
picked" to EEPROM, so a reboot keeps the head empty.

Autoload only listens to the *logical* filament sensors. `reconfigure_sensors_if_needed()`
binds them to the sensors of the currently picked tool; with no tool picked they are
`nullptr`, so an insertion can never trigger autoload. `M1701_autoload` also `bsod`s
("Autoload to notool") if it is ever entered with an empty head.

The hardware can tell which slot got filament: INDX has one side sensor per tool slot
(8, on the XBuddy Extension), and the sensor loop cycles all of them whether or not their
tool is picked. The Filament Color Manager's passive registration already relies on that.

## Design

- **Trigger** (`FilamentSensors::process_events`, not-printing branch, after the stock
  `check_autoload`): when no tool is picked, "Filament Autoloading" is on, no autoload is in
  flight, Marlin is idle and autoload is not locked, the first enabled tool whose enabled
  side sensor reports `filament_inserted` gets `M1701 P1 T<tool>`. `autoload_sent` is set, and
  `M1701_autoload`'s scope guard clears it as before.
- **`P1` = printer tool.** Plain `M1701 T` is a G-code tool and goes through tool mapping,
  which stays active on the Finished/Aborted screen until the print is cleaned up — right when
  the head is empty. `P1` (via `get_target_virtual_from_command_p`) skips the mapping and the
  single-tool collapse; without `P`, M1701 behaves as before. If the tool cannot be resolved,
  `M1701` now clears `autoload_sent` itself, so a failed injection cannot leave autoload (and
  the preheat early-exit that checks it) stuck until reboot.
- **Handling** (`M1701_autoload`): with an empty head, hand off to `M701_load` — the same
  flow as the menu's Load Filament (and the same hand-off stock already uses on bowden
  printers). It asks for the filament type first, so **nothing moves until the user picks a
  type**; Cancel leaves the head alone. Then `Pause::tool_change` picks up the tool
  ("Changing tool" screen), loads, and docks the tool again.
- **Color Manager:** when the empty-head autoload fires, the passive type+color
  registration dialog is not raised for that insertion (the load's preheat prompt already
  offers the color). With autoload off, registration behaves as before.

## Known gaps

- The type prompt of the regular load has no early exit when the filament is pulled back
  out (stock autoload's has one). Pressing a type then still picks up the tool and runs the
  load; Cancel is the way out.
- Only the first slot that triggers in a sensor cycle is loaded; insertions while the trigger
  is blocked (e.g. during another load) are dropped, as with stock autoload.

## Out of scope

- A tool is on the head and filament goes into a *different* slot: unchanged (the Color
  Manager registration dialog, no load).
