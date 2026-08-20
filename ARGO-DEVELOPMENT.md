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
| `v6.6.2-Argo-stable` | Argo features on top of `v6.6.2` (previous working branch; custom-current feature dropped) |
| `v6.6.3` | Stock Prusa release (upstream, untouched) |
| `v6.6.3-Argo-stable` | Argo features on top of `v6.6.3` (previous working branch) |
| `v6.9.0` | Stock Prusa release (upstream, untouched; exactly `refs/tags/v6.9.0`) |
| `v6.9.0-Argo-stable` | **Current working branch** — Argo features rebased onto `v6.9.0` |

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

> **6.6.2 → 6.6.3 note (patch bump):** `v6.6.3` is a direct descendant of `v6.6.2`
> (47 upstream commits). The 39 portable Argo commits were replayed commit-by-commit; the old
> base-specific repair commit was dropped and its still-relevant fixes were re-derived against
> the 6.6.3 APIs. Two replay conflicts needed semantic adaptation: Adaptive Pressure Advance
> retained upstream's `TimeTicks` planner interface, and Prusa's new heater-selftest `M1987`
> kept that number while the Argo Z-endstop calibration moved to the previously unused `M1988`.
> `git range-diff` accounts for every patch: 37 are patch-identical, those two contain only the
> described adaptations, and the old repair is absent. Both `coreone` and `coreone_indx` pass
> the documented Docker build with `-Werror`.

> **6.6.3 → 6.9.0 note (the biggest jump so far):** `v6.9.0` is **not** a descendant of
> `v6.6.3`; the two share the merge base `e96ce2b92` and 6.9.0 carries 783 upstream commits past
> it. The 39 portable Argo commits were replayed commit-by-commit; the old base-specific repair
> commit (`3bf37e497`, subject `v6.6.3`) was dropped and its still-relevant fixes re-derived
> against the 6.9.0 APIs.
>
> **Upstream absorbed the 1.5GT work.** Prusa 6.9.0 ships `HAS_15GT_BELTS()` for the whole Core
> One family: a **Settings → Hardware → "1.5GT Belts"** switch (`MI_BELTS_15GT`) that flips
> `config_store().belts_15gt_installed`, derives X/Y steps/mm from
> `AXIS_STEPS_PER_UNIT_15GT_XY` (101.587 — matching the fork's old hardcoded 101.5873 to within
> 0.0003 steps/mm), and in one transaction invalidates XY homing calibration, the CoreXY grid
> origin, belt tuning and the X/Y axis selftest results. The flag defaults to **off (2GT)**;
> flip it once per printer after flashing. Consequently:
> - `1.5GT FW Core One` and `changed steps for 1.5gt as defaulkt` keep only their
>   build-environment hunks — their `DEFAULT_AXIS_STEPS_PER_UNIT` edits are gone, because that
>   array macro no longer exists (upstream now has `AXIS_STEPS_PER_UNIT_{2GT,15GT}_XY` plus
>   per-axis `DEFAULT_AXIS_STEPS_PER_UNIT_{Z,E0}`).
> - The Advanced Settings **steps/mm block was removed** (`7dd4bdcc9`). Upstream deleted
>   `set_steps_per_unit_x/y` outright and moved the Z setter behind the debug-only
>   `HAS_EXTRA_EXPERIMENTAL_SETTINGS`, and a second unsynchronised X/Y editor sitting next to
>   the belt switch could disagree with it. Advanced Settings keeps its three toggles.
>
> Other adaptations during the replay: Adaptive PA's per-move plumbing was re-merged onto
> upstream's `debug_assert` / `block->print_fan_speed` planner changes; the Filament Color
> Manager's preheat hook moved onto upstream's renamed
> `determine_filament_for_operation(FilamentSelectionArgs)`; `z_endstop_calib_responses` stayed
> in `client_response.hpp` while upstream's `gearbox_alignment_responses` moved out to
> `fsm/gearbox_alignment_phases.hpp`; the web tool-mapping start path now calls
> `marlin_client::print_start(filename, PreviewSkipIfAble::preview)` (upstream renamed
> `print_begin`); and the network-exclusivity pair re-merged against upstream's move from
> `"sockets.h"` to `<lwip/sockets.h>`. `M1988` is still free upstream, so the Z-endstop wizard
> kept that number. Both `coreone` and `coreone_indx` pass the documented Docker build with
> `-Werror`.

---

## Rebase workflow (read before rebasing onto a new Prusa release)

When Prusa publishes the next release, the goal is to re-base the **clean
feature commits** onto it — **not** to carry forward an old `*-Argo-stable` branch with its
repair commit.

### The "rebase repair" convention

A rebase onto a new base can break feature commits in ways that are **specific to that base**
(APIs the feature calls may have changed, stale code may resurface from a bad 3-way merge,
stricter `-Werror`, etc.). Those fixes are collected into a single commit whose subject begins:

> `rebase repair — NOT intended for future cherry-picks / rebases`

Its body lists each fix and the original feature commit it really belongs to. Inspect the
current branch's final repair commit for the concrete fixes needed by this base.

**On the next rebase: DROP the repair commit and re-derive the equivalent fixes against the new
base** (the correct fix may differ). Do not blindly replay it.

### Recommended steps for the next release

1. Fetch and verify the new stock tag from Prusa (`upstream`), then create the matching
   untouched stock branch. Use fully qualified refs because the local branch and tag share a
   name.
2. Rebase the **feature commits only** (the list under "Feature log" below, minus any
   `rebase repair` commit) onto the new stock tag. A practical route is to branch from the
   current Argo tip, run an interactive `git rebase --onto` for the Argo-only range, and mark
   the old repair commit as `drop` in the todo list.
3. Build `coreone` and `coreone_indx` with `-Werror` (see "Build"). Fix what breaks.
4. Collect those fixes into a fresh `rebase repair — NOT intended ...` commit, with a body
   mapping each fix to its origin feature commit.
5. Review the range commit-by-commit and with `git range-diff`, then publish the new
   `<version>-Argo-stable` branch.

> Even cleaner (optional, history rewrite): instead of a separate repair commit, fold each
> repair hunk back into its origin feature commit via interactive rebase, so the feature
> branch stays self-consistent. Only do this on already-shared commits deliberately.

---

## Build

See [`Firmware-Build-Instructions-Docker.md`](Firmware-Build-Instructions-Docker.md). Builds
run in the `prusa-buddy-build:gcc13` Docker image to match Prusa's toolchain.

- Validate firmware changes on **both** `--preset coreone` and `--preset coreone_indx`.
- `-DCUSTOM_COMPILE_OPTIONS:STRING="-Werror"` is the intended strictness. If a fresh rebase
  has pre-existing `-Werror` debt unrelated to your change, that belongs in the rebase-repair
  commit — don't silently drop `-Werror`.

---

## Feature log (Argo additions on top of stock `v6.9.0`)

Oldest → newest. These hashes are from `v6.9.0-Argo-stable` and change on every rebase; obtain
the current range with
`git log --oneline refs/tags/v6.9.0..refs/heads/v6.9.0-Argo-stable`. The `custom current` commit (custom X/Y
motor-current + homing StallGuard-sensitivity editing) was **dropped** at the 6.6.2 rebase.

| Area | Feature | Commit(s) | Notes |
|------|---------|-----------|-------|
| Mechanics | ~~1.5GT belt support + default steps/mm~~ — **superseded upstream** | `a90218cb5`, `e69fbdd88`, `315d253ca`, `7dd4bdcc9` | Since 6.9.0 use **Settings → Hardware → "1.5GT Belts"** (`HAS_15GT_BELTS()`, default off). The three original commits keep only their build-environment hunks and the Advanced Settings screen itself; `7dd4bdcc9` removed the steps/mm items. See the 6.6.3 → 6.9.0 note. |
| Mechanics | Increased XY/Z park speed | `88652d79d` | |
| Chamber | Higher max chamber temp + safety margins | `08677ab62`, `48c004e9b` | up to 65 °C |
| Motion | Adaptive Pressure Advance (no planner flush on `M572 S`) | `383455fa5` | design: [`docs/planning/adaptive-pressure-advance.md`](docs/planning/adaptive-pressure-advance.md) |
| Motion | CoreXY selftest axis-length calibration fix | `317648b84` | adds `phase_stepping::update_axis_motor_params` |
| Homing | Automatic Z-alignment during `G28` | `829854f48` | |
| Network | Wi-Fi / Ethernet mutually exclusive at runtime | `6a5ae02a9`, `ec2a9cc1f` | |
| Filament | Toggle "Preheat & ram before unload" (Advanced Settings): OFF = cold unload, skips BOTH preheat and ramming | `df80d84d1`, `fbffa460e` | ramming skipped in `ram_sequence_process` because Prusa 6.6.0 rams even when cold |
| Filament | Cool down nozzle after load when idle | `f940f1d3b` | |
| Filament | **Filament Color Manager** (per-tool color, autoload prompt, PrusaLink) | `a83f7eaa3`, `74ba815fa` | design: [`docs/planning/filament-color-manager.md`](docs/planning/filament-color-manager.md); user docs: [`doc/filament_color_manager.md`](doc/filament_color_manager.md) |
| Network | **Bed Mesh Viewer** (`GET/POST /api/v1/mesh` + embedded `mesh.html` heatmap with "Run bed leveling") | `df33a0be6`–`4d5492aaf` | design: [`docs/planning/bed-mesh-viewer.md`](docs/planning/bed-mesh-viewer.md); served at `http://<printer>/mesh.html` |
| Network | **PrusaLink Chamber Temperature** (Dashboard sidebar row below Heatbed) | `38268adbb` | `temp_chamber`/`target_chamber` in `/api/v1/status` (`HAS_CHAMBER_API()` guard) + web bundle telemetry map + `index.html` row; design: [`docs/planning/prusalink-chamber-temperature.md`](docs/planning/prusalink-chamber-temperature.md) |
| Calibration | **Z endstop calibration** (Control → Calibrations & Tests → "12 Z endstop calibration") | `04ecb7889` | `HAS_Z_ENDSTOP_CALIBRATION()` option (Core One family); M1988 wizard homes → explicit `calib_Z` → probes one point per Z motor → shows heights + spread with Try again / Quit; design: [`docs/planning/z-endstop-calibration.md`](docs/planning/z-endstop-calibration.md) |
| Chamber | **Filtration only while printing** (no fan during filament load/unload or chamber pre-heat) | `2cb2648c6` | Restores pre-BFW-7026 gate in `ChamberFiltration::needs_filtration()` (`is_printing_state() && planner.max_printed_z > 0`); undoes upstream `a97474829` + `74b85ea7b` side effect that spun the xBuddy-Ext filtration fan whenever the nozzle was hot (incl. attributing previously-loaded ASA via `for_tool_heuristic`); design: [`docs/planning/chamber-filtration-printing-gate.md`](docs/planning/chamber-filtration-printing-gate.md) |
| Network | **PrusaLink temperature control** (click Nozzle/Heatbed sidebar rows → preset + numeric target) | `4abd9e3b4` | `POST /api/v1/printer/{nozzle,bed}/<°C>` (clamp 295/115, `set_target_*`) + `link-controls.js` floating dropdown; no main-bundle patch; design: [`docs/planning/prusalink-temperature-control.md`](docs/planning/prusalink-temperature-control.md) |
| Network | **PrusaLink chamber light switch** (sidebar on/off toggle for the side LED) | `4abd9e3b4` | `POST /api/v1/printer/chamber-light/<0\|1>` via `SideStripHandler` (RAM-shadow restore) + `chamber_light` in `/api/v1/status` (`HAS_SIDE_LEDS()` guard); design: [`docs/planning/prusalink-chamber-light.md`](docs/planning/prusalink-chamber-light.md) |
| Network | **PrusaLink per-tool Filament row** (sidebar; type in tool's Color-Manager color, adapts to 1/4/8 tools) | `4abd9e3b4` | Frontend-only: `link-controls.js` consumes existing `GET /api/v1/filament` (already one entry per `PhysicalToolIndex::count`); swatch+type chips, empty slots shown; design: [`docs/planning/prusalink-filament-tools.md`](docs/planning/prusalink-filament-tools.md) |
| Network | **PrusaLink tool color picker** (click a tool's swatch → pick from the 15 printer colors) | `4abd9e3b4` | `GET /api/v1/filament` gains a `palette` array (`filament_renderer`, single source of truth); picker `PUT /api/v1/filament/<tool>` `{color:NAME\|null}`; design: [`docs/planning/prusalink-filament-color-picker.md`](docs/planning/prusalink-filament-color-picker.md) |
| Network | **PrusaLink web tool mapping** (browser version of the LCD Tools-Mapping screen: after an OrcaSlicer upload-and-print, a mapping-capable print now HOLDS at the `tools_mapping` preview; map G-code filaments → tools + spool join in an overlay, then Print) | `7a357bc6f` | `GET/PUT /api/v1/mapping` + `POST /api/v1/mapping/{confirm,cancel}` (`nhttp/tool_mapping_renderer.*`, `tool_mapping_command.*`); web start-path holds via `PreviewSkipIfAble::preview` (`wui_api.cpp`); frontend `src/resources/web/tool-mapping.js` overlay; design: [`docs/planning/prusalink-tools-mapping.md`](docs/planning/prusalink-tools-mapping.md) |
| Filament | **Single-tool collapse** (a single-enabled-tool machine resolves EVERY G-code tool to its one tool + skips the tool-count/wrong-filament preview blocks, so a multi-filament G-code prints mono-color instead of the "Not enough tools" abort) | `7a357bc6f` | `get_virtual_tool_from_command` → `single_enabled_tool()` (`gcode.cpp`) + suppress fatal `not_enough_tools` (`gcode_compatibility.cpp`) + skip wrong-filament screen (`marlin_print_preview.cpp::stateFromFilamentType`); all gated on `single_enabled_tool()`; needed because `ToolMapper` is a strict bijection (can't collapse many→one); **needs a real print to validate**; same design doc |
| Filament | **TPU-safe INDX tool lock** (lock/unlock E moves run at 10 mm/s when the tool holds a flexible filament) | `2d2e55347` | On INDX the E motor drives the head clamp, so a park pulls 12.5 mm and a pickup pushes 12.3 mm of filament at 35–40 mm/s — 4× Prusa's own flexible ramming feedrate. `e_lock_feedrate()` in `toolchanger_indx.cpp` picks `E_FLEXIBLE_LOCK_FEEDRATE` per tool via `FilamentType::for_tool_heuristic().parameters().is_flexible`; non-flexible tool changes are unchanged. Distances untouched — they are clamp travel, and the release point is not recorded anywhere (no lock sensor, only induction nozzle-presence). |
| Network | PrusaLink dashboard polling hardening | `b857f5e4a` | protects Prusa Connect by reducing and serializing status polling |
| — | **Rebase repair (drop on next rebase)** | `a52622186` | 6.9.0-specific API/build fixes; see "Rebase workflow" |

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
