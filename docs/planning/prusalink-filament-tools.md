# PrusaLink Dashboard — per-tool Filament row — planning notes

> **Status: implemented.** Design record for a "Filament" sidebar row on the PrusaLink web
> Dashboard that shows each tool's filament type in that tool's assigned color (from the
> Filament Color Manager), adapting to the printer's tool count (1 on Core One, up to 8 on
> INDX). Builds on [`prusalink-temperature-control.md`](prusalink-temperature-control.md) /
> [`prusalink-chamber-light.md`](prusalink-chamber-light.md) (same `link-controls.js` module)
> and reuses the [Filament Color Manager](filament-color-manager.md) API. Fork state:
> [`ARGO-DEVELOPMENT.md`](../../ARGO-DEVELOPMENT.md).
>
> **Files:** `src/resources/web/link-controls.js` (render + poll),
> `src/resources/web/index.html` (sidebar row). **Frontend-only — no firmware change.**

## Goal

PrusaLink's sidebar showed Nozzle Diameter but not what filament is loaded. Add a "Filament"
row showing, per tool, the type (PLA, PETG, …) with a swatch in the tool's Color-Manager
color — and make it reflect the real tool count (1 / 4 / 8).

## Why no firmware change was needed

The Filament Color Manager already serves exactly this data at `GET /api/v1/filament`
(`lib/WUI/nhttp/filament_renderer.cpp`), and its renderer iterates
`PhysicalToolIndex::count`, so the response already contains **one entry per physical tool**:

```json
{ "tools": [
  { "tool": 0, "type": "PLA",  "color": "Galaxy Black", "color_rgb": "#1a1a1a" },
  { "tool": 1, "type": null,   "color": null,           "color_rgb": null }
]}
```

So tool-count detection is free: the UI renders whatever tools the endpoint returns. On a
single-tool Core One it's one entry; on a 4- or 8-tool INDX it's 4/8.

## Frontend — `link-controls.js`

- New static `.tel-prop` row in `index.html` (`id="tel-filament"`, after Nozzle Diameter) with
  a spool icon, "Filament" label, and an empty `#lc-filament` flex container. Ships
  `display:none`; revealed once the endpoint returns tools (so it self-hides if the Color
  Manager API is absent).
- `pollFilament()` fetches `/api/v1/filament` on load and every 8 s (filament changes rarely —
  load/unload/autoload). `renderFilament(tools)` builds one **chip per tool**: a color swatch +
  the type name in normal readable light text.
- Chips are built with `createElement` + `textContent` (never `innerHTML`) so the
  network-supplied type/color strings can't inject markup; `color_rgb` is validated against
  `/^#[0-9a-f]{6}$/i` before being used as a CSS value.

## Decisions (from the interview)

- **Single row, chips inline** wrapping — scales 1→8 without a tall sidebar (vs. one row per tool).
- **Swatch + readable text**, not colored type text — dark colors (e.g. black `#1a1a1a`) stay
  legible on the dark sidebar. (Filled auto-contrast chips were the third option.)
- **All tool slots shown, including empty ones** — the user wants the tool count visible
  (1/4/8). Empty slots (`type: null`) render as a muted dashed swatch + "—".
- **Type only** (no color name) for compactness on multi-tool.
- **Tool number prefix when multi-tool** (`tools.length > 1`): each chip is prefixed with its
  1-based tool number so a 4-/8-tool INDX clearly reads as that many tools. Omitted for a
  single tool. (Implementation choice beyond the four interview questions; easy to drop.)
- **No color for a loaded-but-uncolored tool** → neutral grey swatch (`#888`); the type still shows.

## Not done / future

- Could merge filament fetching into the 3 s status poll, or push tool count via `/api/v1/info`.
- Clicking a tool chip could deep-link to its Color-Manager entry (`PUT /api/v1/filament/<tool>`),
  but that's out of scope here.
