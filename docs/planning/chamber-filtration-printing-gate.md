# Chamber filtration: restrict to actual printing — planning notes

> **Status: implemented, build-validated (coreone, -Werror).** Single-file fork
> customization in `src/feature/chamber_filtration/chamber_filtration.cpp`. Current state:
> [`ARGO-DEVELOPMENT.md`](../../ARGO-DEVELOPMENT.md).

## Objective
Stop the chamber / filtration fans (Core One xBuddy Extension) from turning on during
**standalone filament load/unload** and during **chamber pre-heat**. Filtration should run
only while actually printing (fumes are only produced once hot plastic is being extruded),
matching the firmware's behavior before Prusa's 2025/2026 refactors.

## Background — what upstream changed
Two stock Prusa commits combined to cause the regression:

1. **`a97474829` "chamber_filtration: Tie to nozzle temperature" (BFW-7026)** — landed in this
   branch 2025-10-31. Replaced the old "only filter while printing" gate
   (`is_printing_state(...) && planner.max_printed_z > 0`) with a check based purely on
   **real-time nozzle temperature**. After this, any hot nozzle (incl. load/unload preheat and
   chamber pre-heat) could trigger filtration.

2. **`74b85ea7b` "Employ FilamentType::for_tool_heuristic" (BFW-8829)** — 2026-06-02. Changed
   the filament lookup inside `needs_filtration()` from `config_store().get_filament_type()`
   (current filament only) to `FilamentType::for_tool_heuristic()`, which falls back to the
   **previously loaded** filament (`get_previous_filament_type`) when nothing is currently
   loaded. This is why loading *after* ASA spins the fan: the heuristic attributes the
   leftover ASA (which `requires_filtration`).

## Decision — the fix
Re-introduce the original printing gate at the top of `ChamberFiltration::needs_filtration()`:

```cpp
if (!marlin_server::is_printing_state(marlin_vars().print_state.get()) || planner.max_printed_z <= 0) {
    return false;
}
```

- `is_printing_state(...)` → false during standalone load/unload and chamber pre-heat from the
  menu (those happen in `Idle`/preview states). Mid-print **M600** filament changes happen in
  a printing state, so filtration correctly continues there.
- `planner.max_printed_z <= 0` → keeps the fans off during the chamber/bed heat-up *before the
  first extrusion* of a print (the original comment: "we don't want the filtering fans to slow
  down the chamber heat-up").
- All other semantics are preserved: `chamber_filtration_always_on` and the `M147`/`M148`
  override are per-print concepts anyway, so scoping them to the printing state is consistent.

Requires `#include <module/planner.h>` for the `planner` global.

This is the minimal change: it does not touch the per-tool temperature/filament loop, the
post-print filtration timer, or filter-usage accounting — it only restores *when* filtration
is allowed to engage.

## Rebase note
This re-applies behavior that stock firmware removed. On a future rebase onto a newer Prusa
release, check whether upstream still ties filtration purely to nozzle temperature; if so,
re-apply this gate (the exact `is_printing_state` / `max_printed_z` API may have moved).

## Implementation status
- [x] Done — `src/feature/chamber_filtration/chamber_filtration.cpp`
- [x] Build-validated: coreone, `-Werror`
