# PrusaLink Dashboard — Chamber Light on/off switch — planning notes

> **Status: implemented.** Design record for adding a "Chamber Light" on/off switch to the
> PrusaLink web Dashboard sidebar, driving the Core One's side LED strip. Companion feature:
> [`prusalink-temperature-control.md`](prusalink-temperature-control.md); they share the
> `link-controls.js` module and the `printer/` endpoint branch. Fork state in
> [`ARGO-DEVELOPMENT.md`](../../ARGO-DEVELOPMENT.md).
>
> **Files:** `lib/WUI/link_content/prusa_link_api_v1.cpp` (POST endpoint),
> `lib/WUI/nhttp/status_renderer.cpp` (`chamber_light` status field),
> `src/resources/web/link-controls.js` (switch logic),
> `src/resources/web/index.html` (sidebar row), `src/resources/CMakeLists.txt`.
> Build-validated (coreone, `-Werror`).

## Goal

The Core One has an enclosure/side LED ("Chamber Lights" in the on-screen menu). It could be
toggled only from the printer's screen. Add a simple **on/off switch** to the PrusaLink
Dashboard sidebar.

## Firmware — control + state

LEDs are driven by `leds::SideStripHandler` (`include/leds/side_strip_handler.hpp`), the same
object the on-screen *Chamber Lights* menu writes. `set_max_brightness(uint8_t)` is the
persistent brightness (0 = off, 255 = full; stored in `config_store().side_leds_max_brightness`).

**Endpoint** (`prusa_link_api_v1.cpp`, in the shared `printer/` branch):

```
POST /api/v1/printer/chamber-light/<0|1>
```

- **On:** if currently off, restore brightness. The persisted "off" is `max_brightness == 0`,
  so the previous level is remembered in a **RAM shadow** of the last on-brightness; on switch-on
  we restore it, falling back to **full (255)** when the shadow is empty (e.g. after a
  reboot-while-off). This realises the interview choice "ON = restore the menu's configured
  brightness (fallback full)".
- **Off:** remember the current brightness into the shadow, then `set_max_brightness(0)`.
  Because that writes `config_store`, **off persists across reboot** and stays in sync with the
  on-screen menu.
- Whole branch is wrapped in `#if HAS_SIDE_LEDS()`, so it compiles out on printers without the
  strip.

**State** (`status_renderer.cpp`): `/api/v1/status` `printer` object gains, guarded by
`HAS_SIDE_LEDS()`:

```cpp
JSON_FIELD_BOOL("chamber_light", leds::SideStripHandler::instance().get_max_brightness() > 0)
```

so the switch reflects reality and stays in sync if the light is changed from the printer's
screen.

## Frontend — sidebar switch

- A new static `.tel-prop` row in `index.html` (`id="tel-light"`, after Chamber Temperature) with
  a CSS pill switch (`#lc-light-switch`). It ships **`display:none`** and is revealed by JS only
  once `/api/v1/status` reports a `chamber_light` field — so on a build/printer without the LED
  (no field) the row stays hidden.
- `link-controls.js` wires click/Enter/Space → `POST .../chamber-light/<0|1>`, updates the switch
  optimistically, and reconciles from the 3 s status poll (a `lightPending` flag avoids the poll
  fighting an in-flight toggle). A toast confirms ("Chamber light on/off").

## Decisions

- **ON restores configured brightness, OFF persists as 0.** Chosen over "always full" and over a
  transient `M151` override: the switch is the same persistent setting as the menu, survives
  reboot, and stays in sync. Fallback to full only when no prior level is known.
- **RAM shadow, not a new config key.** Avoids touching the `config_store` schema; the only cost
  is that a reboot-while-off forgets the exact previous level and turns on at full — acceptable.
- **Capability-gated UI.** Row hidden unless `chamber_light` is present in status, so the feature
  is self-disabling on unsupported hardware even though this fork only ships Core One.
- **On/off only** (no brightness slider), per the request.

## Not done / future

- A brightness slider could reuse the same endpoint with a 0–100 value instead of 0/1.
- The firmware parts could upstream; the JS module can't (vendored bundle — see companion doc).
