# PrusaLink Dashboard — Chamber Temperature row — planning notes

> **Status: implemented.** Design record for adding a "Chamber Temperature" telemetry
> row to the PrusaLink web Dashboard sidebar, below Heatbed. Part of the broader
> [`PLANS-Web.md`](../../PLANS-Web.md) web-modernization effort. Current fork state:
> [`ARGO-DEVELOPMENT.md`](../../ARGO-DEVELOPMENT.md).
>
> **Files:** `lib/WUI/nhttp/status_renderer.cpp` (firmware web API), the patched web
> bundle `src/resources/web/main.<hash>.js` (telemetry mapping), `src/resources/web/index.html`
> (sidebar row + bundle `<script src>`), `src/resources/CMakeLists.txt` (gzip resource name).
> Build-validated (coreone, `-Werror`).

## Goal

The CoreOne has an actively-controlled chamber; its temperature was already on the
printer's own screen and in the Prusa Connect telemetry, but **not** in the PrusaLink
local web UI. Add it to the Dashboard sidebar so it shows alongside Nozzle/Heatbed.

## Data flow

The sidebar is a static list of `.tel-prop` rows in `index.html`. Each value element
carries `data-where="telemetry.<path>"`; the JS bundle's `updateTelemetry(e)` builds the
`telemetry` object from the `/api/v1/status` JSON response and the renderer walks those
paths. So three layers had to agree:

1. **Firmware web API** — `StatusRenderer::renderState` in `status_renderer.cpp` emits the
   `printer` object (`temp_bed`, `temp_nozzle`, `axis_z`, …). Added, right after
   `target_nozzle` and guarded by `HAS_CHAMBER_API()`:

   ```cpp
   #if HAS_CHAMBER_API()
       JSON_FIELD_FFIXED("temp_chamber",   buddy::chamber().current_temperature().value_or(0), 1) JSON_COMMA;
       JSON_FIELD_FFIXED("target_chamber", buddy::chamber().target_temperature().value_or(0),  1) JSON_COMMA;
   #endif
   ```

   `HAS_CHAMBER_API()` is on for XL / COREONE / COREONE_INDX / COREONEL(+INDX)
   (`ProjectOptions.cmake`), i.e. exactly the printers that have a chamber. Source is the
   same `buddy::chamber()` accessor Prusa Connect already uses (`src/connect/marlin_printer.cpp`).
   `value_or(0)` mirrors Connect: a missing reading renders as 0 (and is then hidden by the
   frontend, see below). `src/` is already on lib/WUI's include path (it uses
   `<state/printer_state.hpp>`), so `<feature/chamber/chamber.hpp>` resolves.

2. **JS bundle** (`updateTelemetry`) — added `chamber` next to `bed` in the temperature map:

   ```js
   bed:{current:e.temp_bed,target:e.target_bed},chamber:{current:e.temp_chamber,target:e.target_chamber},…
   ```

   The bundle is a prebuilt, minified upstream artifact patched in place (no webpack
   re-run); after editing, re-hash and rename it — see web `README.md`.

3. **Dashboard HTML** — a new `.tel-prop` row after Heatbed with
   `data-where="telemetry.temperature.chamber.current"` (and `.target`), `data-format="temp"`,
   `data-zeroes="hide"` (so a 0 current / unset target is blank, same as Heatbed). A small
   inline-SVG enclosure-with-thermometer icon (white + Prusa orange `#FA6831`) matches the
   other rows.

## Decisions

- **No `data-label` / i18n key.** The bundle's translator renders the *raw key* for any
  locale key it doesn't have, and the per-language strings are inlined arrays — adding
  `prop.temp-chamber` across every language is fragile. The row uses static
  "Chamber Temperature" text, which the translator (`[data-label]:not([data-label=""])`)
  leaves untouched. Revisit only if the upstream locale files gain the key.
- **Current + target** (not current-only) to stay consistent with the Heatbed/Nozzle rows;
  target auto-hides when 0.
- **Compile-time guard, not runtime.** The sidebar row is always present in the static HTML;
  on a non-chamber build the field is simply never emitted and the value shows "NA". This
  fork only ships coreone, so in practice it always populates.

## Not done / future

- The C++ field could go upstream to Prusa-Firmware-Buddy as-is. The web part can't: upstream
  vendors the bundle from [Prusa-Link-Web](https://github.com/prusa3d/Prusa-Link-Web), so the
  proper upstream path is a source change there + their rebuild, not a hand-patched bundle.
