# Argo fork — development guide

Start-here notes for working on the **`*-Argo-stable`** branches of this Prusa-Buddy fork.
This is the durable entry point: branch model, the rebase workflow, what has been added on
top of stock Prusa firmware, and where to continue. Per-feature design records live in
[`docs/planning/`](docs/planning/); user-facing feature docs live in [`doc/`](doc/).

---

## Branch model

| Branch | Meaning |
|--------|---------|
| `v6.5.7` | Stock Prusa release (upstream, untouched) |
| `v6.5.7-Argo-stable` | Argo features on top of `v6.5.7` (last known-good before the 6.6.0 bump) |
| `v6.6.0` | Stock Prusa release (upstream, untouched) |
| `v6.6.0-Argo-stable` | Argo features on top of `v6.6.0` (previous working branch) |
| `v6.6.1` | Stock Prusa release (upstream, untouched) |
| `v6.6.1-Argo-stable` | Argo features on top of `v6.6.1` (previous working branch) |
| `v6.6.2` | Stock Prusa release (upstream, untouched) |
| `v6.6.2-Argo-stable` | **Current working branch** — Argo features rebased onto `v6.6.2` (custom-current feature dropped) |

The `*-Argo-stable` branches carry the same set of personal features, re-based onto each new
stock Prusa release.

> **6.6.0 → 6.6.1 note (patch bump):** `v6.6.1` is a direct descendant of `v6.6.0`
> (16 upstream commits, mostly INDX-only). All 38 Argo commits replayed onto `v6.6.1` with a
> clean `git rebase --onto` (zero conflicts; `git range-diff` shows every commit identical).
> Because none of the files touched by the `rebase repair` commit changed between 6.6.0 and
> 6.6.1, the repair commit was **carried unchanged** here (the "drop and re-derive" rule below
> is for larger base changes). Validated with a green `coreone` `-Werror` Docker build.

> **6.6.1 → 6.6.2 note (patch bump):** `v6.6.2` is a direct descendant of `v6.6.1` (32 upstream
> commits — nozzle-cleaner tuning, `M600` parking, INDX/toolchanger, temperature, translations).
> The Argo commits replayed onto `v6.6.2` with conflicts only where upstream touched the same
> code: the Filament Color Manager's `pause.cpp` purge hook (merged with upstream's new
> `nozzle_cleaner_purge_sequence`; color is now persisted only after a successful purge) and the
> tool-mapping `not_enough_tools` check in `gcode_compatibility.cpp` (kept alongside the new
> `HAS_INDX()` indx-lock check). The `rebase repair` commit again applied **unchanged**. The
> **`custom current` feature was intentionally dropped** at this rebase — it added custom X/Y
> motor-current *and* X/Y homing StallGuard-sensitivity editing to Advanced Settings and is no
> longer needed. Dropping it also removed its Advanced-Settings submenu reshuffle; the remaining
> toggles (preheat, cooldown, Z-align) and the steps/mm items sit in the **flat Advanced Settings
> menu** created by the steps/mm commit (`MI_ADVANCED_SETTINGS`, still registered in
> `screen_menu_settings.hpp`). Validated with green `coreone` and `coreone_indx` `-Werror`
> Docker builds.

---

## Rebase workflow (read before rebasing onto a new Prusa release)

When Prusa publishes the next release (e.g. `v6.7.0`), the goal is to re-base the **clean
feature commits** onto it — **not** to carry forward an old `*-Argo-stable` branch with its
repair commit.

### The "rebase repair" convention

A rebase onto a new base can break feature commits in ways that are **specific to that base**
(APIs the feature calls may have changed, stale code may resurface from a bad 3-way merge,
stricter `-Werror`, etc.). Those fixes are collected into a single commit whose subject begins:

> `rebase repair — NOT intended for future cherry-picks / rebases`

(example on this branch: `1437929f3`). Its body lists each fix and the original feature commit
it really belongs to.

**On the next rebase: DROP the repair commit and re-derive the equivalent fixes against the new
base** (the correct fix may differ). Do not blindly replay it.

### Recommended steps for the next release

1. Fetch the new stock tag/branch from Prusa (`upstream`), e.g. `v6.7.0`.
2. Rebase the **feature commits only** (the list under "Feature log" below, minus any
   `rebase repair` commit) onto `v6.7.0`. Easiest: branch the clean feature set from
   `v6.5.7-Argo-stable` or cherry-pick the feature commits; do **not** start from
   `v6.6.0-Argo-stable`'s repair commit.
3. Build `coreone` and `coreone_indx` with `-Werror` (see "Build"). Fix what breaks.
4. Collect those fixes into a fresh `rebase repair — NOT intended ...` commit, with a body
   mapping each fix to its origin feature commit (same format as `1437929f3`).
5. Tag the result `v6.7.0-Argo-stable`.

> Even cleaner (optional, history rewrite): instead of a separate repair commit, fold each
> repair hunk back into its origin feature commit via interactive rebase, so the feature
> branch stays self-consistent. Only do this on already-shared commits deliberately.

---

## Build

See [`Firmware-Build-Instructions-Docker.md`](Firmware-Build-Instructions-Docker.md). Builds
run in the `prusa-buddy-build:gcc13` Docker image to match Prusa's toolchain.

- Validate features on `--preset coreone`. (Previously both `coreone` and `coreone_indx` were
  built, but the maintainer doesn't use INDX, so `coreone` alone is sufficient as of
  2026-06-27. Build `coreone_indx` only if a change specifically touches INDX-only paths.)
- `-DCUSTOM_COMPILE_OPTIONS:STRING="-Werror"` is the intended strictness. If a fresh rebase
  has pre-existing `-Werror` debt unrelated to your change, that belongs in the rebase-repair
  commit — don't silently drop `-Werror`.

---

## Feature log (Argo additions on top of stock `v6.6.2`)

Oldest → newest. The hashes below are illustrative (carried over from a prior rebase) and change
on every rebase — for the exact `v6.6.2-Argo-stable` hashes run
`git log --oneline v6.6.2..v6.6.2-Argo-stable`. The `custom current` commit (custom X/Y
motor-current + homing StallGuard-sensitivity editing) was **dropped** at the 6.6.2 rebase.

| Area | Feature | Commit(s) | Notes |
|------|---------|-----------|-------|
| Mechanics | 1.5GT belt support + default steps/mm | `2004bc8c6`, `b02dd2bcf`, `d4fef8444` | `DEFAULT_AXIS_STEPS_PER_UNIT`, "steps/mm" setting |
| Mechanics | Increased XY/Z park speed | `ad7a600d6` | |
| Chamber | Higher max chamber temp + safety margins | `3219a6253`, `98f9b13ae` | up to 65 °C |
| Motion | Adaptive Pressure Advance (no planner flush on `M572 S`) | `ac4705d68` | design: [`docs/planning/adaptive-pressure-advance.md`](docs/planning/adaptive-pressure-advance.md) |
| Motion | CoreXY selftest axis-length calibration fix | `5859d3784` | adds `phase_stepping::update_axis_motor_params` |
| Homing | Automatic Z-alignment during `G28` | `9430ee442` | |
| Network | Wi-Fi / Ethernet mutually exclusive at runtime | `3a5ecf6c3`, `04f1a78ea` | |
| Filament | Toggle "Preheat & ram before unload" (Advanced Settings): OFF = cold unload, skips BOTH preheat and ramming | `b07303141` (+ ramming skip) | ramming skipped in `ram_sequence_process` because Prusa 6.6.0 rams even when cold |
| Filament | Cool down nozzle after load when idle | `4b09b1751` | |
| Filament | **Filament Color Manager** (per-tool color, autoload prompt, PrusaLink) | `2eb5432e7` | design: [`docs/planning/filament-color-manager.md`](docs/planning/filament-color-manager.md); user docs: [`doc/filament_color_manager.md`](doc/filament_color_manager.md) |
| Network | **Bed Mesh Viewer** (`GET/POST /api/v1/mesh` + embedded `mesh.html` heatmap with "Run bed leveling") | `801f77f55`, `37f11dacb`, `5569b4670`, `15a40c465` | design: [`docs/planning/bed-mesh-viewer.md`](docs/planning/bed-mesh-viewer.md); served at `http://<printer>/mesh.html` |
| Network | **PrusaLink Chamber Temperature** (Dashboard sidebar row below Heatbed) | _(this commit)_ | `temp_chamber`/`target_chamber` in `/api/v1/status` (`HAS_CHAMBER_API()` guard) + web bundle telemetry map + `index.html` row; design: [`docs/planning/prusalink-chamber-temperature.md`](docs/planning/prusalink-chamber-temperature.md) |
| Calibration | **Z endstop calibration** (Control → Calibrations & Tests → "12 Z endstop calibration") | _(uncommitted)_ | `HAS_Z_ENDSTOP_CALIBRATION()` option (Core One family); M1987 wizard homes → explicit `calib_Z` → probes one point per Z motor → shows heights + spread with Try again / Quit; design: [`docs/planning/z-endstop-calibration.md`](docs/planning/z-endstop-calibration.md) |
| Chamber | **Filtration only while printing** (no fan during filament load/unload or chamber pre-heat) | _(uncommitted)_ | Restores pre-BFW-7026 gate in `ChamberFiltration::needs_filtration()` (`is_printing_state() && planner.max_printed_z > 0`); undoes upstream `a97474829` + `74b85ea7b` side effect that spun the xBuddy-Ext filtration fan whenever the nozzle was hot (incl. attributing previously-loaded ASA via `for_tool_heuristic`); design: [`docs/planning/chamber-filtration-printing-gate.md`](docs/planning/chamber-filtration-printing-gate.md) |
| Network | **PrusaLink temperature control** (click Nozzle/Heatbed sidebar rows → preset + numeric target) | _(uncommitted)_ | `POST /api/v1/printer/{nozzle,bed}/<°C>` (clamp 295/115, `set_target_*`) + `link-controls.js` floating dropdown; no main-bundle patch; design: [`docs/planning/prusalink-temperature-control.md`](docs/planning/prusalink-temperature-control.md) |
| Network | **PrusaLink chamber light switch** (sidebar on/off toggle for the side LED) | _(uncommitted)_ | `POST /api/v1/printer/chamber-light/<0\|1>` via `SideStripHandler` (RAM-shadow restore) + `chamber_light` in `/api/v1/status` (`HAS_SIDE_LEDS()` guard); design: [`docs/planning/prusalink-chamber-light.md`](docs/planning/prusalink-chamber-light.md) |
| Network | **PrusaLink per-tool Filament row** (sidebar; type in tool's Color-Manager color, adapts to 1/4/8 tools) | _(uncommitted)_ | Frontend-only: `link-controls.js` consumes existing `GET /api/v1/filament` (already one entry per `PhysicalToolIndex::count`); swatch+type chips, empty slots shown; design: [`docs/planning/prusalink-filament-tools.md`](docs/planning/prusalink-filament-tools.md) |
| Network | **PrusaLink tool color picker** (click a tool's swatch → pick from the 15 printer colors) | _(uncommitted)_ | `GET /api/v1/filament` gains a `palette` array (`filament_renderer`, single source of truth); picker `PUT /api/v1/filament/<tool>` `{color:NAME\|null}`; design: [`docs/planning/prusalink-filament-color-picker.md`](docs/planning/prusalink-filament-color-picker.md) |
| Network | **PrusaLink web tool mapping** (browser version of the LCD Tools-Mapping screen: after an OrcaSlicer upload-and-print, a mapping-capable print now HOLDS at the `tools_mapping` preview; map G-code filaments → tools + spool join in an overlay, then Print) | _(uncommitted)_ | `GET/PUT /api/v1/mapping` + `POST /api/v1/mapping/{confirm,cancel}` (`nhttp/tool_mapping_renderer.*`, `tool_mapping_command.*`); web start-path holds via `PreviewSkipIfAble::preview` (`wui_api.cpp`); frontend `src/resources/web/tool-mapping.js` overlay; design: [`docs/planning/prusalink-tools-mapping.md`](docs/planning/prusalink-tools-mapping.md) |
| Filament | **Single-tool collapse** (a single-enabled-tool machine resolves EVERY G-code tool to its one tool + skips the tool-count/wrong-filament preview blocks, so a multi-filament G-code prints mono-color instead of the "Not enough tools" abort) | _(uncommitted)_ | `get_virtual_tool_from_command` → `single_enabled_tool()` (`gcode.cpp`) + suppress fatal `not_enough_tools` (`gcode_compatibility.cpp`) + skip wrong-filament screen (`marlin_print_preview.cpp::stateFromFilamentType`); all gated on `single_enabled_tool()`; needed because `ToolMapper` is a strict bijection (can't collapse many→one); **needs a real print to validate**; same design doc |
| — | **Rebase repair (drop on next rebase)** | `1437929f3` | see "Rebase workflow" |

---

## Open / future work

- **Prusa Link web modernization** — [`PLANS-Web.md`](PLANS-Web.md): Bed Mesh API + viewer
  **done** (`801f77f55`, `37f11dacb`); G-code console etc. still open. The Filament Color
  Manager's `GET/PUT /api/v1/filament` endpoints
  (`lib/WUI/nhttp/filament_renderer.*`, `filament_command.*`, routed in
  `prusa_link_api_v1.cpp`) are a working template for that plan's "Phase 1" backend endpoints
  (segmented JSON renderer + body-parsing PUT handler + `handler.h` variant registration +
  explicit `SendJson<>` instantiation in `send_json.cpp`).

---

## Conventions / where to continue

- Per-feature **design records** → `docs/planning/` (archived when done, with a status banner).
- Per-feature **user docs** → `doc/`.
- Keep stock Prusa files (`README.md`, `LICENSE.md`, upstream sources) untouched where possible
  so rebases stay clean.
- Commit messages follow the repo's conventional style (`feat(...)`, `fix(...)`); the
  rebase-repair commit is the deliberate exception (see above).
