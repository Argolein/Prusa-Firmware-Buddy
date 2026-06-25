# Filament Color Manager

Lets the user assign a **color** (cosmetic metadata) to the filament loaded in each tool,
in addition to the filament **type**. Targeted at Core One and Core One INDX, but works on
other printers too.

## Enabling

`Settings → Hardware → Filament Color Manager` (on/off). When disabled, the "Type and color"
menu and the autoload color prompt are hidden, and all stored colors are wiped.

## Storage

- Type: reuses the existing per-tool loaded filament type (`config_store.loaded_filament_type`).
- Color: `config_store.filament_color`, a per-tool `uint8_t` index into
  `filament_color_presets` (`src/common/utils/color.cpp`); `255` means "no color".
  The index is persisted, so the palette order must not change (append only).

The color is cleared automatically when filament is unloaded / removed (it is reset in the
`set_filament_type(tool, none)` path).

## Palette (15 colors)

`BLACK, BLUE, GREEN, BROWN, PURPLE, GRAY, TERRACOTTA, SILVER, GOLD, RED, PINK, ORANGE,
TRANSPARENT, YELLOW, WHITE` — defined in `filament_color_presets`.

## UI

- `Filament → Type and color`: per-tool screen with a filament type selector and a color
  selector (rendered as a colored swatch). On a single-tool printer it opens the tool screen
  directly; on multitool it shows a tool list first.
- Autoload: after the filament type is chosen during a load, the user is asked for a color
  (new `PhasesPreheat::user_color_selection` phase). The chosen color is stored on successful
  load.
- INDX only: inserting filament into a **non-selected** tool's sensor while idle opens the
  per-tool Type+Color screen for that tool (no motion, no preheat). This is GUI-driven via a
  pending-request flag set by the filament-sensor handler.

## PrusaLink HTTP API (`/api/v1`)

Authenticated like all other `/api/v1` endpoints.

### `GET /api/v1/filament`

Returns the type and color for every tool.

```json
{
  "tools": [
    { "tool": 0, "type": "PLA", "color": "RED", "color_rgb": "#ff0000" },
    { "tool": 1, "type": null,  "color": null,  "color_rgb": null }
  ]
}
```

- `tool` — physical tool index.
- `type` — filament type name, or `null` if none is loaded/assigned.
- `color` — palette color name (e.g. `"RED"`), or `null`.
- `color_rgb` — hex color string (`#rrggbb`), or `null`. Read-only convenience field.

### `PUT /api/v1/filament/<tool>`

Sets the type and/or color for one tool. Body (both fields optional):

```json
{ "type": "PLA", "color": "RED" }
```

- `type` — a known filament type name, or `null` to clear it.
- `color` — a palette color name, or `null` to clear it.
- Unknown filament types or color names are rejected with `400`.
- An out-of-range `<tool>` returns `404`.
- Success returns `204 No Content`.

Note: a stored color is only shown on the printer when the Filament Color Manager is enabled.
