# PrusaLink Dashboard — Nozzle / Heatbed temperature control — planning notes

> **Status: implemented.** Design record for making the PrusaLink web Dashboard's
> "Nozzle Temperature" and "Heatbed" sidebar rows *interactive* — click a row to set
> the target from a preset list or a typed value. Companion feature:
> [`prusalink-chamber-light.md`](prusalink-chamber-light.md). Part of the broader
> [`PLANS-Web.md`](../../PLANS-Web.md) web-modernization effort; fork state in
> [`ARGO-DEVELOPMENT.md`](../../ARGO-DEVELOPMENT.md).
>
> **Files:** `lib/WUI/link_content/prusa_link_api_v1.cpp` (new POST endpoints),
> `src/resources/web/link-controls.js` (new standalone UI module),
> `src/resources/web/index.html` (`<script>` tag), `src/resources/CMakeLists.txt`
> (gzip resource). Build-validated (coreone, `-Werror`).

## Goal

Until now the PrusaLink local web UI was read-only for temperatures: the sidebar showed
nozzle/bed current+target but offered no way to change them (unlike Prusa Connect, which
has a preset dropdown). Add a Connect-style control so the targets can be set locally,
from presets or a typed value, with the interface limited to **295 °C nozzle / 115 °C bed**.

## Why this differs from the Chamber Temperature row

Chamber Temperature ([`prusalink-chamber-temperature.md`](prusalink-chamber-temperature.md))
was a pure *telemetry* addition — a `/api/v1/status` field plus a static display row, with a
one-line patch to the minified bundle's telemetry map. Temperature *control* needs two new
things the telemetry path doesn't have:

1. **Write endpoints** on the firmware (set the target), and
2. **Interactive UI** (a dropdown, a numeric input, click handlers).

Rather than patch the prebuilt, minified upstream React bundle (`main.<hash>.js`) — which
can't be hooked cleanly — the UI ships as a **separate ES module** (`link-controls.js`),
exactly like `mesh-view.js`. The bundle is left untouched, so **no re-hash / rename** of
`main.*.js` is needed this time.

## Firmware — write endpoints

`PrusaLinkApiV1::accept` (`prusa_link_api_v1.cpp`) gains a `printer/` branch:

```
POST /api/v1/printer/nozzle/<°C>   → set_target_nozzle(clamp(0,295), <all physical tools>)
POST /api/v1/printer/bed/<°C>      → set_target_bed(clamp(0,115))
```

- The requested value rides in the **URL path** (parsed with `from_chars_light`), so the
  handler stays body-less like the existing `mesh` POST / `job` PUT commands — no new
  body-parsing `Handler` variant to register.
- **Server-side clamp is the real safety boundary** (`std::clamp` to 0–295 / 0–115); the
  client-side limits are UX only and must not be trusted.
- Uses the typed `marlin_client::set_target_nozzle / set_target_bed` setters (same ones the
  on-screen Temperature menu and Cooldown use), not string-formatted G-code. Nozzle loops
  `PhysicalToolIndex::all()` so it's correct on multi-tool too (Core One has one).
- Returns `204 No Content`. Auth is the standard `parser.check_auth` gate already applied to
  the whole `/api/v1` prefix.
- The existing `target_nozzle` / `target_bed` fields in `/api/v1/status` are reused by the UI
  to pre-fill the input — no status change was needed for this feature.

## Frontend — `link-controls.js`

The telemetry sidebar is **static HTML in `index.html`** that lives *outside* the SPA's
`#root`, so hooks placed on it are never clobbered by route re-renders. The module:

- Locates the nozzle/bed rows by their telemetry spans
  (`[data-where="telemetry.temperature.nozzle.current"]` → `.closest('.tel-prop')`) — so the
  rows themselves needed **no markup change**.
- Makes each row a `role="button"` and, on click/Enter, opens a **floating panel appended to
  `<body>`** (fixed-positioned next to the row) — keeping it out of any React-managed subtree.
- The panel lists the firmware presets (below) plus **Cooldown**, and a **digit-only input**
  (`inputmode=numeric`, non-digits stripped, live-clamped to the max) with a **Set** button.
- **Apply model:** clicking a preset sets it immediately and closes; a typed value applies on
  **Enter** or **Set** (the choice made during the interview).
- Polls `/api/v1/status` every 3 s to keep the pre-fill target current (and to drive the light
  switch — see companion doc). Closes on outside-click / Esc / scroll / resize.
- A small toast confirms each change ("Nozzle → 215 °C").

### Presets

Mirror the firmware preheat table (`src/common/filament_presets.cpp`), which is also exactly
what Prusa Connect shows (Connect omits PVB & PA):

| Filament | Nozzle °C | Bed °C |   | Filament | Nozzle °C | Bed °C |
|----------|-----------|--------|---|----------|-----------|--------|
| PLA  | 215 | 60  | | ABS  | 255 | 100 |
| PETG | 230 | 85  | | HIPS | 220 | 100 |
| ASA  | 260 | 100 | | PP   | 240 | 100 |
| PC   | 275 | 100 | | FLEX | 240 | 50  |
| PVB  | 215 | 75  | | PA   | 285 | 100 |

Plus **Cooldown** (target 0). The list is hard-coded in the JS (the firmware table is a
stable `constinit`); a future option is to serve it from a small endpoint to remove the
duplication.

## Decisions

- **Allowed any time, including mid-print** (matches Connect). The running G-code may later
  override the value, and changing nozzle temp mid-print can affect quality — accepted.
- **No main-bundle patch / re-hash.** Separate module + body-of-`index.html` `<script>` only.
- **URL-path value, not a JSON body.** Keeps the handler body-less and simple; the UI owns
  both ends so the encoding is private.
- **Clamp server-side.** Client limits (295/115) are convenience; firmware enforces them.
- **No i18n keys** (static English labels), consistent with the Chamber Temperature row.

## Not done / future

- Could expose the preset table via an endpoint instead of duplicating it in JS.
- The firmware endpoints are generic enough to upstream; the JS module can't (upstream vendors
  the bundle from [Prusa-Link-Web](https://github.com/prusa3d/Prusa-Link-Web)).
