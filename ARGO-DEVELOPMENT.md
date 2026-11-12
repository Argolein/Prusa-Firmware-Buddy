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
| `v6.9.0-Argo-stable` | Argo features on top of `v6.9.0` (previous working branch) |
| `v6.9.1` | Stock Prusa release (upstream, untouched; exactly `refs/tags/v6.9.1`) |
| `v6.9.1-Argo-stable` | Argo features on top of `v6.9.1` (previous working branch) |
| `v8.0.1` | Stock Prusa release (upstream, untouched; exactly `refs/tags/v8.0.1`) |
| `v8.0.1-Argo-stable` | **Current working branch** — Argo features rebased onto `v8.0.1` |

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

> **6.9.0 → 6.9.1 note (patch bump):** `v6.9.1` is a direct descendant of `v6.9.0` (23 upstream
> commits — INDX tool-offset sensor/calibration fixes, parking, filament-preset refactors,
> Core One diagonal-homing currents, and the new INDX **gantry squareness** wizard). All 44 Argo commits replayed onto `v6.9.1` with `git rebase --onto`. The only
> conflict was an add/add on `src/marlin_stubs/M1988.cpp`: upstream 6.9.1 took **`M1988`** for
> the gantry squareness wizard (`indx_gantry_squareness`, Calibrations & Tests snake entry on
> `coreone_indx`). Upstream's `M1988` is kept unchanged, and the Argo Z-endstop wizard moved to
> the previously unused **`M1989`** (`M1989.{hpp,cpp}`, `PrusaGcodeSuite::M1989`, `case 1989`,
> its CMake entry and the menu launcher). The rename is folded into the Z-endstop feature commit
> because it is permanent, not base-specific. Everything else auto-merged, including the
> TPU-safe tool lock next to upstream's new `get_tool_dock_position(tool, check_calibrated)`.
> None of the `rebase repair` commit's files changed between 6.9.0 and 6.9.1, so the repair was
> **carried unchanged** (6.6.1 precedent). `git range-diff` shows 43 commits patch-identical and
> only the Z-endstop commit adapted (the M1989 rename). Both `coreone` and `coreone_indx` pass
> the documented Docker build with `-Werror`.

> **6.9.1 → 8.0.1 note (big jump, color system upstream):** `v8.0.1` is **not** a descendant of
> `v6.9.1`; they share the merge base `a3d6bca45` and 8.0.1 carries 764 upstream commits past
> it. The Argo commits were replayed commit-by-commit with `git rebase --onto`; the old repair
> commit (`1d4db20a1`) was dropped.
>
> **Upstream absorbed the Filament Color Manager's printer side.** Prusa 8.0 stores a color with
> each tool's loaded filament (`FilamentTypeParameters::color`, persisted for the per-tool
> ad-hoc filament as RGB555), asks for it on load (**Filament → Ask Color On Load**), edits it
> under **Loaded Filament → Customize → Color**, takes it from `M865 O` / `M600`, matches it in
> the tool-mapping screen and sends it to Prusa Connect — but PrusaLink exposes no filament or
> color at all. Owner's decision: keep only the web layer, on Prusa's storage. Consequently:
> - The Filament Color Manager commit became **`feat(web): PrusaLink filament type and color
>   API`** — only `filament_renderer.*` / `filament_command.*` and their routes. Dropped: our
>   `config_store.filament_color` palette index, the enable toggle, the palette helpers in
>   `color.*`, the load prompt phase, the "Type and color" screen and the INDX sensor trigger
>   (Prusa has no way to color filament that is not loaded). `807fc5633` (keep color during
>   load) was dropped with it. About 525 lines of ours left the shared preheat / pause /
>   config_store / `color.cpp` files.
> - API: `color_rgb` (`#rrggbb`) is the main field and writable; `color` is a palette name only
>   when the color matches one. The palette is Prusa's printer palette (adds Beige/Turquoise,
>   drops Terracotta/Transparent), copied in `filament_renderer.h` — re-check it against
>   `src/gui/menu_item/menu_item_color.cpp` on every rebase. Colors saved under 6.9.1-Argo do
>   not carry over.
> - Web tool mapping reads the color from the tool's filament parameters (now per virtual tool,
>   so MMU slots get colors too).
>
> Other adaptations: the **Z-align-on-G28** feature now calls Prusa's new unattended
> `selftest::auto_calib_Z()` (homes Z first, backs off on an obstruction) instead of our
> `show_ui` plumbing in `G162.cpp` / `calibration_z.hpp`, which is gone; it needs
> `HAS_UNEVEN_BED_DETECTION()` (Core One family). The cooldown-after-load hook moved with
> Prusa's `M1701_autoload` into `M1700_1_parse.cpp`; `MI_FOOTER_SETTINGS` moved to its own
> file upstream; upstream adopted our `${CMAKE_OBJCOPY}` change. The Z endstop wizard (`M1989`)
> also switched to `auto_calib_Z()`, because `calib_Z()` lost its `show_ui` flag and always
> opens the selftest screen. The remaining build fixes are in the fresh repair commit.
>
> Follow-ups done at this rebase (Owner-approved): the filament API now lists and accepts every
> enabled virtual tool (MMU slots were unreachable before), the web picker gained a free color
> input, and the history was cleaned of the README build-doc duplicate, the unused
> "system compiler" hunk in `cmake/GccArmNoneEabi.cmake` and the `start.md` churn (the
> "Update start.md" commit disappeared as empty).
>
> **Build:** new image `prusa-buddy-build:gcc13-v8` (see "Build"). `git range-diff` shows the
> adapted commits above; both `coreone` and `coreone_indx` (the 8-tool INDX firmware —
> `EXTRUDERS 9`, 8 + NoTool) pass the Docker build with `-Werror`.

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
run in the `prusa-buddy-build:gcc13-v8` Docker image (built from the 8.0.1
`utils/holly/Dockerfile`, `--platform linux/amd64`: 8.0.1 builds the ESP Wi-Fi SDKs, whose
compilers exist only for x86-64 Linux) to match Prusa's toolchain. From a worktree, also mount
the main repo's `.git` read-only at its own path so the build's `git show` works.

- Validate firmware changes on `--preset coreone_indx` only (Owner, 2026-11-12: the printer is an INDX).
- `-DCUSTOM_COMPILE_OPTIONS:STRING="-Werror"` is the intended strictness. If a fresh rebase
  has pre-existing `-Werror` debt unrelated to your change, that belongs in the rebase-repair
  commit — don't silently drop `-Werror`.

---

## Feature log (Argo additions on top of stock `v6.9.1`)

Oldest → newest. These hashes are from `v6.9.1-Argo-stable` and change on every rebase; obtain
the current range with
`git log --oneline refs/tags/v6.9.1..refs/heads/v6.9.1-Argo-stable`. The `custom current` commit (custom X/Y
motor-current + homing StallGuard-sensitivity editing) was **dropped** at the 6.6.2 rebase.

| Area | Feature | Commit(s) | Notes |
|------|---------|-----------|-------|
| Mechanics | ~~1.5GT belt support + default steps/mm~~ — **superseded upstream** | `cedbc2c06`, `32111e368`, `07968aaa7`, `60ac724c7` | Since 6.9.0 use **Settings → Hardware → "1.5GT Belts"** (`HAS_15GT_BELTS()`, default off). The three original commits keep only their remaining build-environment hunks (the `GccArmNoneEabi.cmake` hunk was removed at 8.0.1) and the Advanced Settings screen itself; `60ac724c7` removed the steps/mm items. See the 6.6.3 → 6.9.0 note. |
| Mechanics | Increased XY/Z park speed | `f48d59158` | |
| Chamber | Higher max chamber temp + safety margins | `66bac0aee`, `3a8cdce40` | up to 65 °C |
| Motion | Adaptive Pressure Advance (no planner flush on `M572 S`) | `9964576ff` | design: [`docs/planning/adaptive-pressure-advance.md`](docs/planning/adaptive-pressure-advance.md) |
| Motion | CoreXY selftest axis-length calibration fix | `f0e919957` | adds `phase_stepping::update_axis_motor_params` |
| Homing | Automatic Z-alignment during `G28` | `1bf50c490` | Advanced Settings toggle; once per Z power cycle, calls Prusa's `selftest::auto_calib_Z()` (since 8.0.1; backs off on an obstruction); needs `HAS_UNEVEN_BED_DETECTION()` |
| Network | Wi-Fi / Ethernet mutually exclusive at runtime | `c80682f87`, `9b01c7481` | |
| Filament | Toggle "Preheat & ram before unload" (Advanced Settings): OFF = cold unload, skips BOTH preheat and ramming | `11ee88ac9`, `78d4afb6a` | ramming skipped in `ram_sequence_process` because Prusa 6.6.0 rams even when cold |
| Filament | Cool down nozzle after load when idle | `e98ca2e70` | |
| Network | **PrusaLink filament type and color API** (`GET /api/v1/filament`, `PUT /api/v1/filament/<tool>` over Prusa's per-tool filament color) | `17537b98e` | Replaces the Filament Color Manager since 8.0.1 (printer side is upstream now). `color_rgb` `#rrggbb` primary + writable, `color` = palette name when it matches; one entry per virtual tool up to the last enabled one (MMU slots included), PUT only to enabled tools; palette copied from `menu_item_color.cpp` into `filament_renderer.h` — re-check on every rebase; user docs: [`doc/filament_color_manager.md`](doc/filament_color_manager.md); old design: [`docs/planning/filament-color-manager.md`](docs/planning/filament-color-manager.md) |
| Network | **Bed Mesh Viewer** (`GET/POST /api/v1/mesh` + embedded `mesh.html` heatmap with "Run bed leveling") | `6b26d3cb2`–`bfbc8aba2` | design: [`docs/planning/bed-mesh-viewer.md`](docs/planning/bed-mesh-viewer.md); served at `http://<printer>/mesh.html` |
| Network | **PrusaLink Chamber Temperature** (Dashboard sidebar row below Heatbed) | `696d2bc20` | `temp_chamber`/`target_chamber` in `/api/v1/status` (`HAS_CHAMBER_API()` guard) + web bundle telemetry map + `index.html` row; design: [`docs/planning/prusalink-chamber-temperature.md`](docs/planning/prusalink-chamber-temperature.md) |
| Calibration | **Z endstop calibration** (Control → Calibrations & Tests → "<N> Z endstop calibration", numbered after the printer's tests) | `3d13401bf` | `HAS_Z_ENDSTOP_CALIBRATION()` option (Core One family); M1989 wizard (was `M1988` until the 6.9.1 rebase) homes → explicit screen-less `auto_calib_Z` (since 8.0.1) → probes one point per Z motor → shows heights + spread with Try again / Quit; design: [`docs/planning/z-endstop-calibration.md`](docs/planning/z-endstop-calibration.md) |
| Chamber | **Filtration only while printing** (no fan during filament load/unload or chamber pre-heat) | `fec4a1c77` | Restores pre-BFW-7026 gate in `ChamberFiltration::needs_filtration()` (`is_printing_state() && planner.max_printed_z > 0`); undoes upstream `a97474829` + `74b85ea7b` side effect that spun the xBuddy-Ext filtration fan whenever the nozzle was hot (incl. attributing previously-loaded ASA via `for_tool_heuristic`); design: [`docs/planning/chamber-filtration-printing-gate.md`](docs/planning/chamber-filtration-printing-gate.md) |
| Network | **PrusaLink temperature control** (click Nozzle/Heatbed sidebar rows → preset + numeric target) | `54a41ff29` | `POST /api/v1/printer/{nozzle,bed}/<°C>` (clamp 295/115, `set_target_*`) + `link-controls.js` floating dropdown; no main-bundle patch; design: [`docs/planning/prusalink-temperature-control.md`](docs/planning/prusalink-temperature-control.md) |
| Network | **PrusaLink chamber light switch** (sidebar on/off toggle for the side LED) | `54a41ff29` | `POST /api/v1/printer/chamber-light/<0\|1>` via `SideStripHandler` (RAM-shadow restore) + `chamber_light` in `/api/v1/status` (`HAS_SIDE_LEDS()` guard); design: [`docs/planning/prusalink-chamber-light.md`](docs/planning/prusalink-chamber-light.md) |
| Network | **PrusaLink per-tool Filament row** (sidebar; type in the tool's filament color, adapts to 1/4/8 tools) | `54a41ff29` | Frontend-only: `link-controls.js` consumes existing `GET /api/v1/filament` (one entry per tool up to the last enabled one); swatch+type chips, empty slots shown; design: [`docs/planning/prusalink-filament-tools.md`](docs/planning/prusalink-filament-tools.md) |
| Network | **PrusaLink tool color picker** (click a tool's swatch → pick from the 15 printer colors or any custom color) | `54a41ff29`, `3bad8a18d` | `GET /api/v1/filament` gains a `palette` array (Prusa's printer palette since 8.0.1); picker `PUT /api/v1/filament/<tool>` `{color:NAME\|null}`; a non-palette color (set on the printer or by G-code) shows its hex; a native color input sends `{color_rgb:"#rrggbb"}`; design: [`docs/planning/prusalink-filament-color-picker.md`](docs/planning/prusalink-filament-color-picker.md) |
| Network | **PrusaLink web tool mapping** (browser version of the LCD Tools-Mapping screen: after an OrcaSlicer upload-and-print, a mapping-capable print now HOLDS at the `tools_mapping` preview; map G-code filaments → tools + spool join in an overlay, then Print) | `314d0575f` | `GET/PUT /api/v1/mapping` + `POST /api/v1/mapping/{confirm,cancel}` (`nhttp/tool_mapping_renderer.*`, `tool_mapping_command.*`); web start-path holds via `PreviewSkipIfAble::preview` (`wui_api.cpp`); frontend `src/resources/web/tool-mapping.js` overlay; design: [`docs/planning/prusalink-tools-mapping.md`](docs/planning/prusalink-tools-mapping.md) |
| Filament | **Single-tool collapse** (a single-enabled-tool machine resolves EVERY G-code tool to its one tool + skips the tool-count/wrong-filament preview blocks, so a multi-filament G-code prints mono-color instead of the "Not enough tools" abort) | `314d0575f` | `get_virtual_tool_from_command` → `single_enabled_tool()` (`gcode.cpp`) + suppress fatal `not_enough_tools` (`gcode_compatibility.cpp`) + skip wrong-filament screen (`marlin_print_preview.cpp::stateFromFilamentType`); all gated on `single_enabled_tool()`; needed because `ToolMapper` is a strict bijection (can't collapse many→one); **needs a real print to validate**; same design doc |
| Filament | **TPU-safe INDX tool lock** (lock/unlock E moves run at 10 mm/s when the tool holds a flexible filament) | `1322a4b61` | On INDX the E motor drives the head clamp, so a park pulls 12.5 mm and a pickup pushes 12.3 mm of filament at 35–40 mm/s — 4× Prusa's own flexible ramming feedrate. `e_lock_feedrate()` in `toolchanger_indx.cpp` picks `E_FLEXIBLE_LOCK_FEEDRATE` per tool via `FilamentType::for_tool_heuristic().parameters().is_flexible`; non-flexible tool changes are unchanged. Distances untouched — they are clamp travel, and the release point is not recorded anywhere (no lock sensor, only induction nozzle-presence). |
| Network | PrusaLink dashboard polling hardening | `b1242bec9` | protects Prusa Connect by reducing and serializing status polling |
| Filament | **INDX empty-head autoload** (inserting filament into a slot while no tool is on the head starts the regular load for that slot: type prompt first, then pick up, load, dock) | `0d9f8de92` | Stock INDX docks the tool after every print/load, which unbinds the logical sensors, so autoload only worked with a tool picked. `check_empty_head_autoload` in `filament_sensors_handler.cpp` injects `M1701 P1 T<slot>`; `M1701` (`M1700_1_parse.cpp`) hands an empty head to `M701_load`. New `P1` = printer tool (skips tool mapping, still active on the Finished screen). Needs "Filament Autoloading" on; design: [`docs/planning/indx-empty-head-autoload.md`](docs/planning/indx-empty-head-autoload.md) |
| Chamber | **Keep the chamber vents as they are after a print** (no end-of-print vent close) | `77dab6229` | Reverts upstream `7190ab828` (BFW-9080, `Chamber::close_vents_after_print()`), which on INDX homed, picked T4 for the vent lever and parked it again after every PLA/PETG print. The next print's `manage_ventilation_state()` still opens/closes as its filament needs; **needs a real print to validate**; design: [`docs/planning/keep-chamber-vents-after-print.md`](docs/planning/keep-chamber-vents-after-print.md) |
| — | **Rebase repair (drop on next rebase)** | `786d25c0f` | 8.0.1-specific API/build fixes (re-derived; the 6.9.x repair was dropped); see "Rebase workflow" |

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
