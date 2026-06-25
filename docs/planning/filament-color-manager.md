# Filament Color Manager — planning notes (ARCHIVED)

> **Status: DONE / merged.** Implemented and build-validated (coreone + coreone_indx,
> -Werror); committed as `25780ffad` ("feat(filament): Filament Color Manager"). Pre-existing
> rebase breakage fixed separately in `f8f75a630`. User-facing docs:
> [`doc/filament_color_manager.md`](../../doc/filament_color_manager.md). This file is the
> design/interview record; not an active plan. Current state:
> [`ARGO-DEVELOPMENT.md`](../../ARGO-DEVELOPMENT.md).

## Objective
Implement a **Filament Color Manager** for Prusa Buddy firmware (focused on Core One
and Core One INDX, must not break other printers). The feature lets the user assign a
filament **color** per tool (cosmetic metadata only), shows type + color in a new menu,
extends the autoload flow to ask for color, and exposes type + color (read/write) to
PrusaLink.

The **filament type** is NOT a new field: it reuses the firmware's existing per-tool
loaded filament type (`config_store.loaded_filament_type`, single source of truth). Only
**color** is new storage.

## Open questions
- None. All clarified in the interview (see Decisions).

## Decisions
- **Toggle:** `filament_color_manager_enabled` (bool) in EEPROM, exposed as a toggle in
  Settings → Hardware. When OFF: hide the "Type and color" menu, skip the autoload color
  prompt, suppress the INDX passive dialog, **and wipe all stored colors**.
- **Type field = real loaded type (synced).** The menu's "Filament type" reads/writes the
  existing `loaded_filament_type` per tool. No separate type storage.
- **Color = cosmetic only.** Never affects temperatures, motion, or any logic. Used only
  for the menu icon and PrusaLink.
- **Color storage:** new `filament_color` `StoreItemArray` in `store_definition.hpp`,
  parallel to `loaded_filament_type` (sized `EXTRUDERS`), storing raw `uint32_t` color
  (or a 4-bit/8-bit palette index — see Approved plan, decided at implementation).
- **Color palette:** fixed list of 15 named colors. ~10 already exist in `color.hpp`.
  Missing — **BROWN, TERRACOTTA, GOLD, PINK, TRANSPARENT** — get sensible RGB defaults
  (TRANSPARENT gets a placeholder rendering, e.g. outlined/checkerboard icon), subject to
  user review.
- **Color lifecycle / clearing:** color clears on filament unload / filament-removed
  sensor event for that tool. Disabling the feature also wipes all stored colors.
- **Phase 2 (Core One / selected tool):** after the existing "filament type?" autoload
  prompt, ask "color?" (only when feature enabled), then continue the normal routine.
- **Phase 2 (INDX, non-selected tool sensor insertion):** passive dialog asks Type then
  Color with **no preheat and no motor movement**. It **does** set that tool's
  `loaded_filament_type` + color in firmware state (motion-free). Appears **only when the
  feature is enabled**. (This supersedes the earlier "metadata only" wording — chosen for
  a clean single-source-of-truth model.)
- **Phase 3 (PrusaLink):** expose type + color per tool, **read and write**, with
  documented naming.

## Approved plan

### Phase 1 — Storage & menus
- [ ] Add `filament_color_manager_enabled` (bool) to `store_definition.hpp` under an
      appropriate `ItemFlag` (hw/printer config), default `false`.
- [ ] Add `filament_color` `StoreItemArray` to `store_definition.hpp`, parallel to
      `loaded_filament_type`, sized `EXTRUDERS`, default = "unset" sentinel.
- [ ] Add accessor helpers (`get_filament_color(tool)` / `set_filament_color(tool, ...)`
      / `clear_filament_color(index)`) mirroring the `*_filament_type` helpers.
- [ ] Define the 15-color palette in `color.cpp`/`.hpp`; add the 5 missing colors and a
      TRANSPARENT rendering decision.
- [ ] Add `MI_FILAMENT_COLOR_MANAGER` (`MItem_toggle`) in `MItem_hardware.hpp`/`.cpp`;
      add it to `screen_menu_hardware.{hpp,cpp}`.
- [ ] Add the `Filament → "Type and color"` entry to `screen_menu_filament.{hpp,cpp}`,
      visible only when the toggle is ON.
- [ ] Create a tool-list screen under `src/gui/screen/filament/` (list tools when
      `count > 1`, else go straight to Tool 0). Per-tool screen shows: Filament type
      (edits `loaded_filament_type`) and Filament color (15-color picker, colored icon).
      Reuse existing `screen_filament_detail` patterns where possible.

### Phase 2 — Autoload enhancements
- [ ] In the autoload type-selection flow (M70X / preheat path), after the type is
      chosen and when the feature is enabled, present a color picker before continuing.
- [ ] INDX: handle filament insertion on a non-selected tool's sensor — show a
      motion-free, preheat-free Type→Color dialog (feature-gated), and set that tool's
      `loaded_filament_type` + color. Respect preset module gating (INDX has no MMU).
- [ ] Clear stored color on unload / filament-removed sensor event per tool.

### Phase 3 — PrusaLink integration
- [ ] Expose per-tool filament type + color in the PrusaLink API
      (`marlin_server_types` / relevant structs), read and write.
- [ ] Wire write path back to `loaded_filament_type` (type) and `filament_color` (color)
      with validation. Document the chosen JSON naming.

## Acceptance checks
- Toggle ON shows menu + autoload color prompt; OFF hides them and wipes colors.
- Color icon renders correctly per tool, including TRANSPARENT placeholder.
- Core One autoload asks type → color → continues normally.
- INDX non-selected-tool insertion shows the dialog with no motion/preheat and stores
  type + color.
- Color clears on unload / filament-removed.
- PrusaLink reports and accepts type + color per tool.
- Builds with `--preset coreone` and `--preset coreone_indx`.

## Risk areas
- INDX passive insertion must not trigger any motion/preheat or interfere with an active
  print/job.
- Preset module gating (MMU not compiled on INDX).
- EEPROM journal migration: add new items without breaking existing `loaded_filament_type`.
- PrusaLink write validation (reject invalid tool index / color / type).

## Implementation status
- [ ] Not started
- [ ] In progress
- [x] Done — all phases implemented and BUILD-VALIDATED with -Werror on both
      `coreone` and `coreone_indx` (2026-06-25).

### Phase 1 — DONE and BUILD-VALIDATED (coreone, -Werror, 2026-06-25)
Builds clean: `coreone_release_boot.bbf`/`.bin` produced with `-DCUSTOM_COMPILE_OPTIONS="-Werror"`.

#### Pre-existing branch bugs fixed to unblock the build (NOT part of the feature; rebase artifacts on v6.6.0-Argo-stable)
- src/gui/CMakeLists.txt: removed stale ref to deleted MItem_basic_selftest.cpp (reintroduced by 890721f20).
- include/marlin/Configuration_COREONE.h:477: `101.5873` -> `101.5873f` (kept value; fixes -Werror=float-conversion; from f0dedbd69b).
- lib/Marlin/.../planner.cpp: removed two spurious `block->extruder = extruder;` blocks (member was removed by da34069c1; resurrected by rebase in e1e470b7dd) and added `#include ".../phase_stepping/axes.hpp"` (a10f4b38e6 call needed it).
- lib/Marlin/.../gcode/calibrate/G28.cpp: removed bogus `#include <buddy/unreachable.hpp>` (bsod_unreachable comes from <bsod.h>); `result.zalign = TestResult_Passed` -> `result.set_zalign(TestResult::passed)` (selftest API changed; from 3cb8c663e).
- src/marlin_stubs/pause/M701_2.cpp (cooldown/preheat commits 3a37b1eb8/106cdb7f9 vs newer base):
  - `is_safely_retracted_for_unload(hotend_from_extruder(...))` -> `can_cold_unload(PhysicalToolIndex::from_raw(hotend_from_extruder(...)))`.
  - removed two redundant `marlin_server::set_temp_to_display(...)` calls (auto-called by setTargetHotend since 6.6).
  - `M70X_process_user_response(..., target_extruder)` -> pass `virtual_tool` (fn now takes VirtualToolIndex).

### Phase 1 — implementation summary
- color.cpp/.hpp: `color_presets` renamed/exported as `filament_color_presets`
  (`extern const std::array<ColorPreset, 15>`), declared in color.hpp. All 15 colors
  already existed in color.cpp (no new RGB values needed).
- store_definition.hpp/.cpp: added `filament_color_manager_enabled` (bool, hash
  "Filament Color Manager Enabled") and `filament_color` (uint8_t[PhysicalToolIndex::count],
  255=unset, hash "Filament Colors" — note: "Filament Color" collided with "Dock Position 2",
  do not use). Helpers: get/set/clear_all_filament_colors. Color cleared in
  set_filament_type(none) path (filament-removed chokepoint).
- MItem_hardware.hpp/.cpp: `MI_FILAMENT_COLOR_MANAGER` toggle; disabling wipes colors.
  Added to screen_menu_hardware.hpp.
- NEW src/gui/screen/filament/screen_type_and_color.{hpp,cpp} (+ CMakeLists): per-tool
  type select (writes loaded_filament_type) and color select (swatch via printExtension,
  reuses frame_tool_mapping drawing). Entry item `MI_TYPE_AND_COLOR` added to
  screen_menu_filament.hpp (hidden when feature disabled); single-tool opens per-tool
  screen directly, multitool opens tool list.

### Phase 2 — DONE and BUILD-VALIDATED (coreone + coreone_indx, -Werror)
- Part A (autoload color prompt): new `PhasesPreheat::user_color_selection` phase
  (preheat_phases.hpp); server color sub-loop in M70X_preheat.cpp (gated to load modes +
  feature enabled via should_ask_filament_color); GUI color menu frame (MI_PREHEAT_COLOR
  swatch items + WindowMenuColor + FrameColorSelection) in screen_preheat.cpp; color
  persisted at load completion (pause.cpp:763) via filament_color_palette_index() reverse
  lookup added to color.cpp/.hpp. Color flows through existing color_to_load.
- Part B (INDX passive non-selected-tool dialog): NO new g-code (per user). HAS_INDX-gated
  check_passive_color_registration() in filament_sensors_handler.cpp detects insertion edge
  on non-selected tools while idle and sets a thread-safe pending request; GUI home-screen
  LOOP consumes it (consume_pending_color_registration) and opens the Phase 1
  ScreenToolTypeAndColor for that tool (writes type+color to config_store). No motion, no
  preheat, no FSM.

### Phase 3 — DONE, BUILD-VALIDATED on coreone (INDX build in progress)
PrusaLink REST, read + write, auth-gated like all /api/v1 endpoints. Documented in
doc/filament_color_manager.md.
- GET /api/v1/filament -> { "tools": [ { tool, type, color, color_rgb }, ... ] }.
  Renderer: NEW lib/WUI/nhttp/filament_renderer.{h,cpp} (segmented JsonRenderer, per-tool
  loop modeled on file_info). Registered SendJson<link_content::FilamentRenderer> in
  handler.h variant + explicit instantiation in send_json.cpp (link step needs it!).
- PUT /api/v1/filament/<tool> body { "type": "PLA"|null, "color": "RED"|null }.
  Handler: NEW lib/WUI/nhttp/filament_command.{h,cpp} (body accumulation modeled on
  JobCommand; parse_command + json::search). Validates type via FilamentType::from_name,
  color via filament_color_palette_index_by_name (added to color.cpp/.hpp). Applies type
  first then color (set_filament_type(none) clears color). 400 on unknown, 404 on bad tool,
  204 on success. Registered printer::FilamentCommand in handler.h variant.
- Routing in prusa_link_api_v1.cpp; sources added to lib/WUI/CMakeLists.txt.

### (history)

## Handoff
- Agent: Claude Code
- Date: 2026-06-25
- Completed this session:
  - Requirements interview + finalized spec (rewrote this PLANS file).
  - Implemented all of Phase 1 (storage, palette export, settings toggle, "Type and color"
    menu). BUILD-VALIDATED on coreone with -Werror.
  - Fixed pre-existing branch compile bugs (rebase artifacts) to get a green build —
    see "Pre-existing branch bugs fixed" above.
  - Mapped Phase 2 (preheat FSM: ScreenPreheat / handle_filament_selection / M70X) and
    Phase 3 (PrusaLink: lib/WUI/link_content/basic_gets.cpp JSON "material" field).
- Stopped at: ALL PHASES COMPLETE. Phases 1, 2 (A+B), 3 implemented and build-validated
  with -Werror on coreone + coreone_indx. Plus fixed pre-existing branch build bugs.
- Next step: User review + commit. Suggest committing the pre-existing-branch-bug fixes
  separately from the feature. No on-hardware testing has been done (builds only).
- Open blockers: none.
- Decisions made this session: See Decisions section.

## Notes
- Compile with `--preset coreone` or `--preset coreone_indx`.
- Docker build works via image `prusa-buddy-build:gcc13` and the documented `docker run`;
  builds green with `-DCUSTOM_COMPILE_OPTIONS:STRING="-Werror"` as of 2026-06-25.
- Reference branch for the pre-rebase original code: remote ref/v6.6.0
  (github.com/Argolein/Prusa-Firmware-Buddy tree v6.6.0).
- Journal hashes are auto-generated at build time from store_definition.hpp; new
  StoreItem strings must not hash-collide (SHA-256[:4] & 0x3FFF). Verified "Filament Colors"
  and "Filament Color Manager Enabled" are collision-free.
- Existing per-tool type lives in `config_store.loaded_filament_type`
  (`store_definition.hpp`); reuse it — do not duplicate type storage.
