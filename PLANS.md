# PLANS.md

## Objective
Expose Core One steps-per-mm editing in the normal Settings menu through a new "Advanced Settings" screen. X/Y/Z/E values should be editable with two decimals, stored directly in Prusa `config_store`, and applied immediately to the running planner without requiring `M500`, a separate save action, or a reboot.

## Open questions
- none

## Approved plan
- Add a normal visible `Settings > Advanced Settings` menu entry.
- Add an Advanced Settings screen containing X/Y/Z/E steps-per-mm controls.
- Reuse Prusa `config_store` steps-per-unit storage as the persistent source of truth.
- When a value changes, write it to `config_store` and update `planner.user_settings`/`planner.apply_settings()`/`planner.refresh_positioning()` immediately.
- Keep the accepted UI value range at `1.00` to `1000.00` and display two decimals.
- Do not implement or change `M500`/`M501` for this work.

## Implementation status
- [ ] Not started
- [ ] In progress
- [x] Done

## Decisions
- Do not implement `M500`; persistence is handled through the display menu.
- Do not add a "Save to EEPROM" menu item.
- The display menu stores changes directly and persistently.
- Changed steps-per-mm apply immediately without reboot.
- The new menu should be normally visible as `Settings > Advanced Settings`.
- The steps-per-mm UI should display and edit values with two decimal places, e.g. `101.59`.
- The accepted UI range is `1.00` to `1000.00`.

## Handoff
- Agent: Codex
- Date: 2026-04-26
- Completed this session:
  - Added a visible `Settings > Advanced Settings` menu entry.
  - Added X/Y/Z/E steps-per-mm controls with two-decimal editing and range `1.00` to `1000.00`.
  - Wired the controls to store directly into Prusa `config_store` and immediately update planner settings.
  - Built COREONE successfully with Docker/GCC13 and `-Werror`.
- Stopped at:
  - Build artifact generated in `build/products-docker-gcc13-advanced-settings/coreone_release_boot.bbf`.
- Next step:
  - Flash the BBF and verify that X/Y/Z/E values can be changed under `Settings > Advanced Settings`, survive reboot, and are reflected by `M92` output.
- Open blockers:
  - none
- Decisions made this session:
  - No M500/M501 work; direct persistent UI storage with immediate planner apply.

## Notes
- Existing Prusa storage paths found so far include direct `config_store` writes for Input Shaper and existing steps-per-unit setters in `store_c_api.cpp`.
- `WiSpin` already supports float values and fixed decimal rendering through `NumericInputConfig::max_decimal_places`.
- The earlier Core One 1.5GT homing/selftest fix is complete and separate from this Advanced Settings work.
