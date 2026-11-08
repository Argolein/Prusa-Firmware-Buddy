# Filament color over PrusaLink

Since 8.0, Prusa firmware stores a **color** with the filament loaded in each tool. This fork
adds what Prusa leaves out: reading and setting that color (and the filament type) over the
local PrusaLink API, without Prusa Connect.

## Prusa's color model (what the printer does)

- The color is part of the filament parameters (`FilamentTypeParameters::color`).
- Only the per-tool **ad-hoc** filament (Prusa's "custom copy" of a preset) can hold a color.
  Setting a color on a tool loaded with a preset (say `PLA`) turns it into that tool's ad-hoc
  copy, the same as **Loaded Filament → Customize** on the printer.
- The color is lost when the filament is unloaded.
- On the printer: the color prompt on load (**Filament → Ask Color On Load**),
  **Loaded Filament → Customize → Color**, `M865 O"#rrggbb"`, and the `M600` color parameter.

## PrusaLink HTTP API (`/api/v1`)

Authenticated like all other `/api/v1` endpoints.

### `GET /api/v1/filament`

Returns the type and color for every tool.

```json
{
  "tools": [
    { "tool": 0, "type": "PLA", "color": "Red", "color_rgb": "#ff0000" },
    { "tool": 1, "type": "PETG", "color": null, "color_rgb": "#3a7bd5" },
    { "tool": 2, "type": null, "color": null, "color_rgb": null }
  ]
}
```

- `tool` — tool index (MMU slot on an MMU printer), 0-based. Tools are listed up to the
  last enabled one; a disabled tool inside that range shows `type: null`.
- `type` — filament type name, or `null` if none is loaded.
- `color_rgb` — the stored color as `#rrggbb`, or `null`. This is the main field.
- `color` — the palette name when `color_rgb` is a palette color, else `null`.

The printer stores colors with 5 bits per channel, so a color can read back slightly
different from what was sent (`#808080` reads back as `#848484`).

### `PUT /api/v1/filament/<tool>`

Sets the type and/or color for one tool. Body (all fields optional):

```json
{ "type": "PLA", "color_rgb": "#ff8800" }
```

- `type` — a known filament type name, or `null` to clear it.
- `color_rgb` — any color as `#rrggbb`, or `null` to clear it.
- `color` — a palette name (case-insensitive, e.g. `"red"`), or `null` to clear it.
- The type is applied first, so a type and a color can be set in one request.
- A color for a tool without a filament type returns `409`.
- Unknown filament types, palette names or malformed hex values return `400`.
- An out-of-range or disabled `<tool>` returns `404`.
- Success returns `204 No Content`.

## Palette

The 15 colors of the printer's own color menu: Black, White, Gray, Red, Orange, Yellow, Green,
Blue, Purple, Pink, Brown, Beige, Turquoise, Gold, Silver. Defined for PrusaLink in
`lib/WUI/nhttp/filament_renderer.h` as a copy of `src/gui/menu_item/menu_item_color.cpp`;
re-check the copy when Prusa changes its palette.
