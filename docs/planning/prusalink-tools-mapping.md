# PrusaLink Dashboard — G-code-filament → tool mapping over the web — planning notes

> **Status: IMPLEMENTED (uncommitted), pending Docker build + hardware validation of Phase 2.**
> Design record for exposing the printer's existing **Tools Mapping** screen (the two-column
> "G-Code filaments ↔ Printer tools" preview) to the PrusaLink web UI, so that after an
> OrcaSlicer upload-and-print the user can map filaments to tools and start the print **from the
> browser** — mirroring what the LCD already offers when you select a G-code at the printer. Fork
> state: [`ARGO-DEVELOPMENT.md`](../../ARGO-DEVELOPMENT.md). Builds on the
> [Filament Color Manager](filament-color-manager.md) (per-tool type + color) and the existing
> PrusaLink control layer ([`link-controls.js`](../../src/resources/web/link-controls.js)).
>
> **Files.** *Backend:* `lib/WUI/nhttp/tool_mapping_renderer.{h,cpp}` (GET),
> `lib/WUI/nhttp/tool_mapping_command.{h,cpp}` (PUT), routes + confirm/cancel in
> `lib/WUI/link_content/prusa_link_api_v1.cpp`, variant in `lib/WUI/nhttp/handler.h`,
> `SendJson<>` in `send_json.cpp`, sources in `lib/WUI/CMakeLists.txt`, start-path in
> `lib/WUI/wui_api.cpp`. *Phase 2:* `lib/Marlin/Marlin/src/gcode/gcode.cpp`
> (`get_virtual_tool_from_command`). *Frontend:* `src/resources/web/tool-mapping.js` + a `<script>`
> in `index.html`, registered in `src/resources/CMakeLists.txt`.

## Goal

In OrcaSlicer, point the **Device-UI URL** at the printer's PrusaLink page (e.g.
`http://<printer>/#print`). Upload-and-start a print. Instead of the print auto-starting (and
silently ignoring multi-filament intent), the browser shows a mapping screen: the G-code's
filaments (index + type + color swatch, from the sliced file) on one side, the printer's tools
(index + loaded type + Color-Manager color) on the other, with a per-filament **dropdown** to
choose the target tool. Confirm → the print starts with that mapping applied.

Works for:
- **Core One + MMU3** (5 slots) and **Core One INDX** (4/8 tools) — *Phase 1*.
- **Bare single-extruder Core One** (no MMU/INDX): load a multi-filament G-code and collapse
  every filament onto the one tool — *Phase 2* (needs a core-gate change + hardware validation).

## Interview decisions (2026-07-02)

- **Architecture — "hold at preview".** Reuse the real firmware print-preview FSM: the web
  start-path stops in the existing `tools_mapping` phase instead of auto-skipping it. The web is
  a *remote control of the same screen the LCD shows* — same `GCodeInfo`, same `tool_mapper`,
  same FSM response — so LCD and web stay in sync and there is one source of truth. (Rejected
  alternative: browser scans the G-code itself and sends start+mapping together, Connect-style —
  duplicates the preview scan.)
- **UI — dropdowns**, not a pixel-faithful two-column line-connect widget. One row per G-code
  filament (index + type + swatch) with a tool `<select>` (each option showing the tool's type +
  color). Mobile-friendly; delivered as a new static module like `link-controls.js` / `mesh-view.js`.
- **v1 scope includes** tool type+color display, **type-compatibility warnings** (G-code PLA
  mapped to a tool loaded with PETG), and **spool-join** (chain a tool to continue on another).
- **Firmware-only.** No OrcaSlicer code changes — the user sets the Device-UI URL in OrcaSlicer.
- **Both hardware targets** (MMU/INDX *and* bare single tool) are wanted; delivered as two phases.

## Firmware findings (grounded)

Confirmed by reading the tree; these are the load-bearing facts for the design.

- **The machinery is compiled into every Core One build.** `ProjectOptions.cmake:564` sets
  `HAS_MMU2` for `COREONE`, and `HAS_TOOL_MAPPING`/`HAS_SPOOL_JOIN` follow (line 783). So
  `tool_mapper`, `spool_join`, `FrameToolMapping`, and the `tools_mapping` FSM phase all exist on
  `coreone`, `coreone_indx`, MMU and non-MMU alike. This is a **web-expose**, not new machinery.
- **The mapping brain:** `tool_mapper` singleton (`set_mapping(gcode→virtual)`,
  `set_all_unassigned`, `set_enable`, `to_virtual`/`to_gcode`, `reset`) —
  `lib/Marlin/Marlin/src/module/prusa/tool_mapper.{hpp,cpp}`; `spool_join`
  (`add_join`, `get_spool_2`, chains) — `.../prusa/spool_join.hpp`.
- **G-code filament list** (left column: type + color) = `GCodeInfo` per-extruder info
  (`filament_name`, `extruder_colour`, `used()`), `src/common/gcode/gcode_info.hpp`. It is
  populated **for free** by the preview scan — a strong reason to go through the preview FSM.
- **Printer tools list** (right column: type + color) = `config_store().get_filament_type(tool)`
  + the Argo per-tool color (`get_filament_color`), the same data the Color-Manager `GET
  /api/v1/filament` already renders (`lib/WUI/nhttp/filament_renderer.*`).
- **Where the preview holds / proceeds:** `src/common/marlin_print_preview.cpp`.
  `State::tools_mapping_wait_user` ↔ `PhasesPrintPreview::tools_mapping` (line 81). At line 707
  it enters that state when `tools_mapping::is_tool_mapping_possible()` and the caller did not ask
  to skip mapping.
- **Skip levels:** `PreviewSkipIfAble` (`src/common/marlin_events.h:57`) —
  `no < preview < tool_mapping < all`. The web start-path currently uses **`all`** (skip
  everything, including mapping); **`preview`** skips only the thumbnail and *holds* at mapping.
- **The mapping gate:** `tools_mapping::is_tool_mapping_possible()`
  (`src/common/tools_mapping.cpp:20`) returns **false** on a Core One when the MMU is not enabled
  (`#if HAS_MMU2()` → `if (!MMU2::mmu2.Enabled()) return false;`). This is why the bare-single-tool
  case is Phase 2 — see below. With MMU enabled or on INDX it is true when the G-code uses >1
  extruder, or >1 tools are enabled and the G-code uses ≥1.
- **The headless-drive pattern already exists.** Connect's `start_print` accepts a `tool_mapping`
  2D array → `handle_tool_mapping()` (`src/connect/marlin_printer.cpp:477`): `set_all_unassigned`
  → `set_enable(true)` → `set_mapping` per entry → `spool_join.add_join`. And `dialog_action`
  (line 623) peeks the FSM state (`marlin_vars().peek_fsm_states`), validates the phase, and sends
  `FSM_encoded_response`. **These two functions are the exact template for the new endpoints.**
- **Threading.** The LCD `FrameToolMapping` mutates `tool_mapper`/`spool_join` **directly** from
  the display thread (`frame_tool_mapping.cpp` lines 51/183/196/416/…) and confirms with
  `marlin_client::FSM_response(PhasesPrintPreview::tools_mapping, response)` (lines 828/839). It
  relies on the marlin thread being blocked awaiting the FSM response during this phase, so no
  lock is taken. Connect touches the same globals from its own thread. The httpd/WUI thread —
  which already calls `print_begin`/marlin_client — may do the same. (Simultaneous LCD-and-web
  editing is a rare edge case; both act on the same singletons, last-write-wins, consistent with
  the existing lock-free design.)

## Architecture — hold at preview

```
OrcaSlicer  ──upload+print──▶  POST /api/v1/files/<path>  (Print-After-Upload)
                                      │  wui_start_print(autostart=true)
                                      ▼
                          print_begin(path, PreviewSkipIfAble::preview)   ← was ::all
                                      │  preview scans G-code → GCodeInfo
                                      ▼
                is_tool_mapping_possible()? ──no──▶ proceed to print (as today)
                                      │yes
                                      ▼
                        State::tools_mapping_wait_user   ── printer HELD ──┐
                                      ▲                                     │
   browser polls GET /api/v1/mapping ─┘  reads phase + filaments + tools   │
   user maps in dropdowns                                                  │
   PUT /api/v1/mapping {mapping, spool_join}  → tool_mapper/spool_join set │
   POST /api/v1/mapping/confirm  → FSM_response(tools_mapping, Print) ─────┘
                                      ▼
                                  Printing
```

The **only** change to the print flow is the skip level on the web start-path; the FSM does the
rest exactly as it does for the LCD.

### Start-path skip level (regression audited)

`wui_start_print` (`lib/WUI/wui_api.cpp:280`) passes the skip level into `print_begin`. The
autostart branch now passes **`preview`** instead of `all`. Auditing every `skip_if_able`
comparison in `marlin_print_preview.cpp` (lines 335, 416, 430, 713) confirmed only line 713
(`>= tool_mapping`) distinguishes `preview` from `all`; 335/416/430 treat them identically. So the
change **only** makes the flow hold at `tools_mapping` (when `is_tool_mapping_possible()`); no other
preview prompt is reintroduced. Single-tool / single-material web prints behave exactly as before.
A dedicated `all_except_tool_mapping` skip value was therefore **not** needed. Web-start only; the
LCD/USB/Connect start-paths are untouched. Multi-tool web prints now always hold for mapping
confirmation instead of auto-starting when the default 1-1 mapping happened to be compatible —
which is the point of the feature.

## HTTP API (follows the FilamentRenderer / FilamentCommand pattern)

- **`GET /api/v1/mapping`** — `ToolMappingRenderer : JsonRenderer<ToolMappingRenderState>`
  (segmented via `SendJson`; the whole small state is **snapshotted at construction** like
  `MeshRenderState`, so resumes stay consistent and no lock is held while streaming). Body:
  ```jsonc
  {
    "active": true,                 // printer is holding at the tools_mapping phase
    "gcode_filaments": [
      { "index": 1, "type": "PLA", "color_rgb": "#ff6d45", "tool": 1 }, // tool = current mapping, or null
      { "index": 2, "type": "PLA", "color_rgb": "#c8506e", "tool": 2 }
    ],
    "tools": [
      { "index": 1, "enabled": true,  "type": "PLA", "color_rgb": "#…" },
      { "index": 4, "enabled": false, "type": null,  "color_rgb": null }, // empty slot placeholder
      { "index": 5, "enabled": true,  "type": "PLA", "color_rgb": "#…" }
    ],
    "spool_join": [ /* { "from": <tool>, "to": <tool> } … optional chains */ ]
  }
  ```
  When not in the phase → `{ "active": false, "gcode_filaments": [], "tools": [], "spool_join": [] }`
  so the frontend hides/closes the UI. There is no `dialog_id`; the confirm/cancel handlers
  **re-validate the phase server-side** instead (simpler, and still rejects a stale confirm).
  Tool type comes from `config_store().get_filament_type(virtual)`, color from
  `get_filament_color(virtual.to_physical())` (the Argo Color Manager — the LCD leaves tool color
  blank; the web fills it). `to_physical()` is only called for **enabled** tools.
- **`PUT /api/v1/mapping`** — `ToolMappingCommand` (body-parse like `FilamentCommand`, guarded so it
  is only accepted while holding at the phase → else `409`). **Compact positional** body (chosen to
  stay well under the shared `MAX_TOKENS=30` jsmn cap and to parse trivially with the flat event
  callback):
  ```jsonc
  { "mapping":    [1, 1, 2],    // mapping[gcode_raw] = printer tool display index (1-based); 0 = unassigned
    "spool_join": [0, 3, 0] }   // spool_join[tool_raw] = tool to continue on (1-based); 0 = none
  ```
  Applies via the `handle_tool_mapping` sequence (`set_all_unassigned` → `set_enable(true)` →
  `set_mapping` per entry; `spool_join.reset()` → `add_join`). Returns `204`, or `400` on out-of-
  range / invalid mapping / spool-join, or on **any used G-code filament left unmapped** — because
  the mapper is a bijection (below), an accidental collision would otherwise fatal-error at print.
- **`POST /api/v1/mapping/confirm`** — `FSM_response(PhasesPrintPreview::tools_mapping,
  Response::Print)`, after re-checking (via `peek_fsm_states`) that we're still at that phase (like
  Connect's `dialog_action`); else `409`.
- **`POST /api/v1/mapping/cancel`** — same, with `Response::Abort` (the phase's other valid
  response per `PrintPreviewResponses`).

Wiring: `prusa_link_api_v1.cpp` (routes + confirm/cancel helper) · `handler.h`
(`ConnectionState` gains `SendJson<link_content::ToolMappingRenderer>` +
`printer::ToolMappingCommand`) · `send_json.cpp` (explicit `SendJson<ToolMappingRenderer>`) ·
`lib/WUI/CMakeLists.txt` (both new `.cpp`). All mapping routes are `#if HAS_TOOL_MAPPING()`.

## Frontend — `src/resources/web/tool-mapping.js`

- New static module (IIFE, `window.__tmInit` guard), `<script defer src="tool-mapping.js?v=1">`
  added to `index.html`, registered as a gzip resource in `src/resources/CMakeLists.txt`; upstream
  bundle untouched (same layering as `link-controls.js` / `mesh-view.js`).
- Visibility-gated poll of `GET /api/v1/mapping` (4 s) using the same `timedFetch` abort-timeout as
  `link-controls.js` (a stalled socket must not starve Prusa Connect's shared lwIP TCP-PCB pool).
  When `active` and not shown → open a full-screen overlay; while open, polling only auto-**closes**
  it if the phase ends elsewhere (LCD confirm/abort) — it never clobbers in-progress edits.
- Overlay: one row per `gcode_filament` (index + validated `#rrggbb` swatch + type) and a tool
  `<select>` listing each **enabled** tool ("Tool N — TYPE"); preselects the current mapping.
  A `⚠` shows when the chosen tool's type ≠ the G-code filament's type (non-blocking, like the LCD).
- Spool join: a collapsed secondary section, one "when Tool N runs out → continue on …" `<select>`
  per enabled tool; builds the positional `spool_join` array.
- **Duplicate guard:** because the mapper is a bijection, the UI blocks Print with a clear toast if
  two filaments target the same tool (instead of surfacing the backend `400`).
- **Print** → `PUT /api/v1/mapping` then `POST /api/v1/mapping/confirm`; **Back** / backdrop / Esc →
  `POST /api/v1/mapping/cancel` (a stray click cancels, never confirms a print). `#print` hash nudges
  an immediate poll (the OrcaSlicer Device-UI URL), but the overlay only opens when actually holding.
- Security: `color_rgb` validated against `/^#[0-9a-f]{6}$/i` before use as CSS; every network string
  via `textContent`; the only `innerHTML` is a static literal template (like `link-controls.js`).

## Phase 2 — bare single-tool Core One (single-tool collapse)

The interview premise was "load a multi-filament G-code and map every filament onto the one tool."
That is **not** representable through the mapper: `ToolMapper::set_mapping`
(`tool_mapper.cpp`) is a strict **bijection** — mapping a second G-code filament onto a tool
un-maps the first — and an unmapped G-code tool **fatal-errors** at print time (`T.cpp:84`,
`fatal_error("Tool is not mapped")`). So the collapse cannot happen in the mapper or the mapping
screen; it has to happen in **tool resolution**.

Implemented in `GcodeSuite::get_virtual_tool_from_command` (`lib/Marlin/Marlin/src/gcode/gcode.cpp`):
when `VirtualToolIndex::single_enabled_tool()` has a value (exactly one enabled tool — a bare Core
One / MMU-disabled machine), **every** G-code tool resolves to that one tool, turning `T1`/`T2`
into no-op tool changes so a multi-filament G-code prints mono-color instead of fatal-erroring:

```cpp
if (const auto only_tool = VirtualToolIndex::single_enabled_tool(); only_tool.has_value()) {
  return *only_tool;
}
```

**Print-preview gate (found in hardware testing — the resolution change alone was not enough).**
The `get_virtual_tool_from_command` collapse runs at *print* time, but the print-preview blocks
*before* it: on the non-tool-mapping path a multi-tool file on one tool trips the fatal
`not_enough_tools` check (`UsedExtrudersCount > enabled tools`, severity `Abort`) → the ABORT-only
"G-Code incompatibilities detected" screen, plus per-tool `filament_type` warnings. Two scoped
guards, both keyed on `single_enabled_tool().has_value()` (and, for the filament check,
`UsedExtrudersCount() > 1`) so normal single-tool prints are untouched:
- `gcode_compatibility.cpp` (`generate_without_toolmapping`): don't set `not_enough_tools` when
  there is exactly one enabled tool — the collapse handles it, so it must not fatally block.
- `marlin_print_preview.cpp` (`stateFromFilamentType`): skip the wrong-filament screen for a
  multi-tool file on one tool — the type mismatch is inherent to a deliberate mono print (many
  G-code filaments, one loaded). A normal single-tool file still gets the warning.

The filament-*presence* loop needs no change: phantom tools 2..N have no filament sensor
(`GetExtruderFSensor` returns null → `Disabled`, not `NoFilament`), so it doesn't prompt; the real
tool is still checked. Validated with a green `coreone` `-Werror` build; hardware print pending.

Consequences / notes:
- `single_enabled_tool()` returns `nullopt` on MMU/INDX (>1 enabled tool), so those machines are
  **unchanged** — normal mapping applies. The change is scoped to the genuinely-single-tool case.
- A bare single-tool machine keeps `is_tool_mapping_possible() == false`, so **no mapping screen
  appears** for it — there is nothing to map with one tool; the print just proceeds. The web overlay
  is therefore an MMU/INDX feature; Phase 2 is a headless "print a multi-material G-code on one tool"
  capability, not a UI.
- Behavior change: a stray `T1` in a file sent to a single-tool printer no longer fatal-errors — it
  prints on the one tool. That is the requested behavior (accepted trade-off vs. the old sanity
  signal); could be gated behind a config option later if undesired.
- **Hardware-validation caveat:** the change removes the primary blocker (the `T` fatal-error), but
  a multi-material G-code also carries MMU/toolchange-specific commands (wipe-tower moves, `Tx`/`Tc`,
  `M702`, …) that a non-MMU machine may not handle. It **cannot be signed off by the Docker build
  alone** — it needs a real print on a bare single-tool Core One.

## Build / validation

- Docker build on `--preset coreone` with `-Werror` after Phase 1 (must be green; report
  flash/RAM delta). The mapping machinery is present on `coreone`, so Phase 1 is fully
  build-verifiable there. Build `coreone_indx` too only if a change touches INDX-only paths.
- Phase 2: same build, plus a real-hardware print test on a bare single-tool Core One before it is
  considered done (hardware-safety note: touches the print state machine / tool resolution).

## Not done / future

- Live thumbnail/preview of the model in the web mapping screen (out of scope; dropdowns only).
- Editing filament **type** from the mapping screen (the Color-Manager PUT already supports it;
  could be linked later).
- Reflecting web-side edits back to a simultaneously-open LCD screen in real time beyond the
  shared-singleton/last-write-wins behavior.
