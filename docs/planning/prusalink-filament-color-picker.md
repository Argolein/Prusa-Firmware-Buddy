# PrusaLink Dashboard — tool color picker — planning notes

> **Status: implemented.** Design record for making each tool's color swatch in the PrusaLink
> "Filament" row clickable, opening a picker of the printer's 15 built-in filament colors that
> reassigns the tool's color via the Filament Color Manager. Extends
> [`prusalink-filament-tools.md`](prusalink-filament-tools.md); reuses the
> [Filament Color Manager](filament-color-manager.md) `PUT /api/v1/filament/<tool>`. Fork state:
> [`ARGO-DEVELOPMENT.md`](../../ARGO-DEVELOPMENT.md).
>
> **Files:** `lib/WUI/nhttp/filament_renderer.{h,cpp}` (new `palette` array in the GET response),
> `src/resources/web/link-controls.js` (picker UI). Build-validated (coreone, `-Werror`).

## Goal

The Filament row showed each tool's type in its assigned color but the color was read-only.
Make the swatch clickable to reassign the tool's color from the printer's own palette.

## Firmware — palette as single source of truth

The set side already existed: `PUT /api/v1/filament/<tool>` with `{ "color": "<NAME>" }` (or
`null` to clear) maps the name to a palette index (`filament_color_palette_index_by_name`,
`src/common/utils/color.cpp`). What was missing was the **list** of selectable colors for the
picker. Rather than duplicate the 15 colors in JS (drift risk — the EEPROM-persisted index
means order/contents must not change, and names must match exactly for the PUT), the GET
renderer now also emits the palette:

```jsonc
GET /api/v1/filament
{ "tools": [ … ],
  "palette": [ {"name":"BLACK","color_rgb":"#000000"}, … 15 entries … ] }
```

- `FilamentRenderState` gained a second iteration (`palette`, `palette_first`, name/rgb buffers)
  and a `load_palette()`, mirroring the existing per-tool `load()`. The segmented renderer runs
  the palette loop after the `tools` array; loop counters persist across resumes exactly like
  the tools loop.
- The loop bound casts `filament_color_presets.size()` to `uint8_t` to avoid a `-Wsign-compare`
  failure under `-Werror` (the tools loop is safe only because `PhysicalToolIndex::count` is a
  `uint8_t`).
- The 15 colors: BLACK, BLUE, GREEN, BROWN, PURPLE, GRAY, TERRACOTTA, SILVER, GOLD, RED, PINK,
  ORANGE, TRANSPARENT, YELLOW, WHITE.

## Frontend — picker

- `pollFilament()` stores `j.palette`; `renderFilament()` makes the swatch of each **loaded**
  tool a `<button>` (empty slots stay non-interactive, per the interview). Clicking opens a
  floating picker (same `placeNear` positioner as the temperature dropdown) with a 5-column grid
  of the palette swatches plus a dashed **"No color"** entry; the current color is ringed.
- Selecting sends `PUT /api/v1/filament/<tool>` with `{ "color": <NAME|null> }`, toasts, and
  re-polls so the row reflects the change. Picker closes on outside-click / Esc / scroll / resize
  (the existing handlers were generalised to close both panels).
- Swatches are built with `createElement`; `color_rgb` is validated against `/^#[0-9a-f]{6}$/i`
  before use as a CSS value, and names go through DOM properties (`title`), never `innerHTML`.

## Decisions (from the interview)

- **Palette served from firmware**, not hardcoded — single source of truth, no drift, and the
  names are guaranteed to match what the PUT accepts.
- **"No color" included** — clears the assignment (`color: null`).
- **Only loaded tools are editable** — matches "the color left of the filament type"; empty
  INDX slots stay placeholders (avoids the odd "colored but typeless" slot).

## Not done / future

- A combined type+color editor (the PUT also accepts `type`) could let the web set filament type
  too; out of scope here.
- The picker could show color names as labels (currently tooltips) if desired.
