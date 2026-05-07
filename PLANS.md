# PLANS.md

## Objective
Extend the existing `Settings > Advanced Settings` screen into submenus for `Steps/mm` and `Motor currents`. Keep the existing X/Y/Z/E steps-per-mm controls under `Steps/mm`. Add X/Y/Z/E motor current controls under `Motor currents`, showing the current firmware value, allowing user edits, persisting the confirmed value like the steps-per-mm controls, and enforcing the effective Core One TMC2130 upper limit of 958 mA.

## Open questions
- none

## Approved plan
- Keep `Settings > Advanced Settings` visible only for Core One.
- Change Advanced Settings into a submenu screen with `Steps/mm` and `Motor currents`.
- Move the existing X/Y/Z/E steps-per-mm controls under `Steps/mm`.
- Add `Motor currents` controls for X/Y/Z/E using 200 mA to the Core One effective TMC2130 limit of 958 mA in 1 mA steps.
- Show live firmware TMC current values in the motor-current UI.
- On current confirmation, write the value to Prusa `config_store` and apply it immediately to the live TMC driver.
- Add `Reset to defaults` as the last item under `Motor currents`, restoring the firmware default current values and applying them immediately.
- Make Core One TMC initialization use saved current values on boot.
- Keep Prusa's fixed Precise CoreXY homing currents when X/Y motor currents are still at defaults.
- When a custom X/Y current is configured, use that value for the Precise CoreXY measurement current and do not reduce the holding current below the configured value, clamped to the Core One effective TMC2130 limit.
- Invalidate CoreXY precise homing and TMC sensitivity calibration when X or Y current changes.
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
- This Advanced Settings extension is Core One only.
- Motor current values use 200 mA minimum, 958 mA maximum, and 1 mA increments on Core One.
- Motor current edits are saved directly to `config_store` and applied immediately to the live TMC driver.
- Motor current boot initialization must use the saved `config_store` value.
- The `Motor currents` screen includes a `Reset to defaults` action.
- Core One keeps stock Precise CoreXY homing behavior at default X/Y current: measure current `650` mA and holding current `900` mA.
- Core One custom X/Y current overrides Precise CoreXY measurement current for that axis.
- Core One custom X/Y current keeps Precise CoreXY holding current at least at the configured current, clamped to the same `200` to `958` mA range.
- Changing X or Y current clears both CoreXY grid-origin calibration and CoreXY TMC sensitivity calibration so the next precise homing recalibrates for the new current.
- Current save/restore paths should use Marlin's requested current setpoint via `getMilliamps()`, not TMC register readback via `rms_current()`, because register readback is quantized and can turn requested values like `550` into `539` or requested `1000` into `958`.
- On stock Core One X/Y hardware with `RSENSE = 0.22`, the TMC2130 effective RMS current register tops out at about `958` mA even if the requested UI value is higher.
- The Core One motor-current menu caps X/Y/Z/E at `958` mA because all four configured TMC2130 drivers use `RSENSE = 0.22`; boot-time TMC initialization also clamps saved values above `958` mA.
- Core One homing/selftest reset paths should apply the user's configured X/Y/Z currents, clamped at `958` mA, instead of forcing firmware defaults such as `550` mA.
- Core One exposes X/Y homing StallGuard sensitivity in Advanced Settings so custom motors can be tuned when stock `-2` or Precise CoreXY `-6..-4` false-trigger.
- Higher TMC2130 StallGuard sensitivity values are less sensitive in this firmware; early false triggers should be tuned by increasing the value.

## Handoff
- Agent: Codex
- Date: 2026-05-06
- Completed this session:
  - Changed `Settings > Advanced Settings` into a Core-One-only menu entry with `Steps/mm` and `Motor currents` submenus.
  - Moved the existing X/Y/Z/E steps-per-mm controls under `Steps/mm`.
  - Added X/Y/Z/E motor current controls with range `200` to `958` mA.
  - Added `Reset to defaults` under `Motor currents`.
  - Wired motor-current edits and reset to store in Prusa `config_store` and apply immediately to the live TMC drivers.
  - Changed Core One TMC initialization to use saved `config_store` current values on boot.
  - Updated Precise CoreXY homing current handling so stock X/Y current keeps Prusa's `650` mA measurement and `900` mA holding overrides.
  - Updated Precise CoreXY homing current handling so custom X/Y current is used for measurement and holding is not reduced below the configured current.
  - Changed CoreXY calibration clearing to reset both grid-origin and TMC sensitivity calibration.
  - Wired X/Y motor-current changes to clear CoreXY homing calibration.
  - Changed CoreXY homing, generic homing current reset, and phase stepping restore paths to preserve requested current setpoints via `getMilliamps()` instead of quantized TMC readback.
  - Capped the Core One motor-current menu and Core One boot-time TMC initialization at the effective `958` mA TMC2130 limit for `RSENSE = 0.22`.
  - Changed Core One `homing_reset()` current handling so axis selftest/Y calibration applies configured X/Y/Z currents instead of resetting them to firmware defaults.
  - Added `Homing sensitivity` under Core One `Advanced Settings` with X/Y controls and reset to firmware defaults.
  - Wired Core One sensorless homing to use configured X/Y homing sensitivity from `config_store`.
  - Changed Precise CoreXY sensitivity calibration to scan around configured X/Y homing sensitivity when a custom value is set.
  - Ran `git diff --check` successfully.
  - Built Core One successfully in Docker/GCC13 with `-Werror`.
- Stopped at:
  - Build artifact generated at `build/products-docker-gcc13-coreone-motor-currents/coreone_release_boot.bbf` with SHA256 `28a1b3acf1d5755d8b3ebd0c172ff39f332f49494cf834c510e98c18986a5abd`.
- Next step:
  - Flash the BBF and verify `Steps/mm`, `Motor currents`, and a fresh CoreXY homing calibration after changing X/Y current on device.
- Open blockers:
  - none
- Decisions made this session:
  - Scope is Core One only.
  - Motor-current UI range is 200 mA to 958 mA on Core One.
  - Motor-current reset restores firmware defaults and applies them immediately.
  - Custom X/Y currents should be respected by Precise CoreXY homing instead of forcing Prusa's fixed measurement current.
  - X/Y current changes invalidate CoreXY homing calibration because StallGuard sensitivity is current-dependent.
  - Preserve requested current setpoints during temporary current changes; do not persist quantized TMC register readbacks such as `539` or `958` as the user's configured value.
  - Stock Core One X/Y/Z/E0 hardware cannot effectively deliver more than about `958` mA RMS with the configured TMC2130 sense resistor value.
  - Core One axis selftest/homing-reset paths must respect configured X/Y/Z currents; default-current reset remains stock behavior for other printer models.
  - Core One custom X/Y homing sensitivity is persisted in existing `homing_sens_x`/`homing_sens_y` config-store fields; reset stores the firmware-default/unset state.
  - No M500/M501 work; direct persistent UI storage with immediate runtime apply.

## Notes
- Existing Prusa storage paths found so far include direct `config_store` writes for Input Shaper and existing steps-per-unit setters in `store_c_api.cpp`.
- `WiSpin` already supports float values and fixed decimal rendering through `NumericInputConfig::max_decimal_places`.
- The earlier Core One 1.5GT homing/selftest fix is complete and separate from this Advanced Settings work.
