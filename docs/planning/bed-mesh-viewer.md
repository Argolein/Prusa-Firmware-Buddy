# Bed Mesh Viewer (PrusaLink web) — planning notes

> **Status: Phase 1 (backend) + Phase 2 (frontend) implemented.** Design record for a
> bed-mesh heatmap viewer in the PrusaLink web interface. Phase 1 = `GET /api/v1/mesh`
> endpoint + renderer unit test. Phase 2 = the standalone embedded `mesh.html` heatmap page.
> Part of the broader [`PLANS-Web.md`](../../PLANS-Web.md) web-modernization effort. Current
> fork state: [`ARGO-DEVELOPMENT.md`](../../ARGO-DEVELOPMENT.md). User-facing docs (when done):
> `doc/bed_mesh_viewer.md`.
>
> **Phase 1 files:** `src/common/bed_mesh_api.{hpp,cpp}` (pull-based snapshot accessor),
> `lib/WUI/nhttp/mesh_renderer.{h,cpp}` (segmented JSON renderer), route + variant +
> instantiation in `prusa_link_api_v1.cpp` / `handler.h` / `send_json.cpp`, unit test
> `tests/unit/lib/WUI/nhttp/mesh_renderer_tests.cpp` (+ `mesh_mock.cpp`). Build-validated
> (coreone + coreone_indx, `-Werror`); renderer logic verified standalone.
>
> **Phase 2 files:** `src/resources/web/mesh.html` (~23 KB raw, ~6–7 KB gzipped) registered
> in `src/resources/CMakeLists.txt`, served at `http://<printer>/mesh.html`.

## Objective
Show the bed mesh as a **heatmap with variance/quality stats** in the PrusaLink web UI,
similar to Mainsail/Fluidd (Klipper). Focused on Core One / Core One INDX; must not break
other printers. Lightweight: all rendering happens **client-side in the browser**; the MCU
only serializes the existing mesh grid as JSON on demand.

## Hardware feasibility (verified)
Not a CPU/RAM problem. Target board for `coreone` / `coreone_indx` is **XBUDDY → STM32F427ZI**
(168 MHz, 192 KB internal RAM, 2 MB flash, **no external SDRAM**;
[`ProjectOptions.cmake:177`](../../ProjectOptions.cmake),
[`stm32f42x.ld:4-5`](../../src/device/stm32f4/linker/stm32f42x.ld)). Because the heatmap is
drawn in the browser, the MCU never allocates a framebuffer or runs an image codec — it only
streams ~441 floats as JSON via the existing segmented-JSON + HTTP-chunked machinery. The mesh
already lives in RAM. The only real budget to watch is flash for the (small) added web page.

## Data source (verified)
- `ubl.z_values`, type `float[GRID_MAX_POINTS_X][GRID_MAX_POINTS_Y]`
  ([`ubl.cpp:67`](../../lib/Marlin/Marlin/src/feature/bedlevel/ubl/ubl.cpp)).
- Grid is per-printer:
  - **Core One: 21×21** (1,764 B), major points **7×7 = 49 actually probed**, rest interpolated
    ([`Configuration_COREONE.h:1005-1008`](../../include/marlin/Configuration_COREONE.h)).
  - Core One-L: 27×27 (major 9×9); XL: 36×36. Code must read the compile-time grid dims, not
    hardcode 21.
- Existing `ExtUI::onMeshUpdate` callback fires on every mesh point change
  ([`marlin_server.cpp:3944`](../../src/common/marlin_server.cpp)) — currently logs only. Not
  required for the pull-based design below, but available if a push/notify is wanted later.

## Decisions
- **Frontend delivery: standalone embedded page** (not a fork of the React SPA). A single
  self-contained `src/resources/web/mesh.html` (HTML + CSS + `<canvas>` JS, target ~10–15 KB),
  embedded as a firmware resource with one `add_gzip_resource` line, reachable at
  `http://<printer>/mesh.html`, fetching `GET /api/v1/mesh`. Rationale: stays entirely in this
  repo, no Node toolchain, rebase-clean, tiny, full control. Trade-off accepted: not inside the
  PrusaLink SPA navigation (separate page / bookmark), styling is our own.
- **Stats computed in the browser; probed points marked.** Firmware sends the raw grid plus
  the major-point count and grid geometry; the browser computes min / max / range / std-dev and
  visually distinguishes the truly-probed points from interpolated cells. Keeps firmware
  minimal and is honest about interpolated data.
- **Mesh access is pull-based (snapshot), NOT a `marlin_vars` member.** `MarlinVariable`
  asserts lock-free/atomic and forbids structures/arrays
  ([`marlin_vars.hpp:88`](../../src/common/marlin_vars.hpp)), so the 21×21 array cannot live
  there; and a permanent ~1.8 KB mirror would waste RAM when nobody is viewing. Instead a
  `marlin_server` accessor takes the Marlin mutex and `memcpy`s `z_values` into a caller
  buffer on request. (This supersedes PLANS-Web.md's original "store mesh in marlin_vars" note.)
- **Stream row-by-row.** The segmented renderer's connection-state struct holds only one row
  (~84 B) and re-fetches per resume, instead of carrying a full ~1.8 KB snapshot that would
  bloat every connection slot (the `ConnectionState` variant is sized to its largest member).

## Backend plan (mirrors the Filament Color Manager wiring)
1. **Snapshot accessor** in `marlin_server` / `marlin_client`: copy `z_values` + grid geometry
   (points, major points, min/step per axis) under the Marlin lock.
2. **`lib/WUI/nhttp/mesh_renderer.{h,cpp}`** — `JsonRenderer<MeshRenderState>` modeled on
   [`filament_renderer.h`](../../lib/WUI/nhttp/filament_renderer.h); streams rows.
3. **Route** `GET /api/v1/mesh` in
   [`prusa_link_api_v1.cpp:181`](../../lib/WUI/link_content/prusa_link_api_v1.cpp) (new
   `else if (suffix == "mesh")` branch next to `filament`).
4. **Register** `SendJson<MeshRenderer>` in the `ConnectionState` variant
   ([`handler.h:150`](../../lib/WUI/nhttp/handler.h)).
5. **Explicit instantiation** `template class SendJson<MeshRenderer>;` in
   [`send_json.cpp:99`](../../lib/WUI/nhttp/send_json.cpp).
6. **Resource registration** for `mesh.html` in
   [`src/resources/CMakeLists.txt:118`](../../src/resources/CMakeLists.txt).

### Proposed JSON shape
```json
{
  "x_points": 21, "y_points": 21,
  "x_major": 7, "y_major": 7,
  "x_min": 0.0, "x_step": 12.5, "y_min": 0.0, "y_step": 12.5,
  "data": [[ -0.02, 0.01, ... ], ...]
}
```
`data` is `y_points` rows of `x_points` floats. Browser derives stats and the probed-point
mask from `x_major`/`y_major`. Reserve a way to signal "no valid mesh yet" (e.g. empty `data`
or a `valid:false` flag) so the page can show "run a mesh leveling first".

## Frontend (`src/resources/web/mesh.html`) — implemented
Single self-contained file, no external deps (offline on the printer). Design direction: a
**metrology instrument readout** — dark slate chrome, every measurement in tabular monospace.
- Fetches `/api/v1/mesh` with `credentials: "same-origin"` (reuses the browser's cached
  PrusaLink digest creds); 401 / no-mesh / network states each show a dedicated overlay.
- `<canvas>` heatmap: grid painted to an offscreen buffer then scaled with smoothing →
  bilinear interpolation matching the firmware. Diverging color scale (blue→neutral→red).
- **Signature: topographic survey** — contour isolines (marching squares, deviation from mean,
  emphasized zero line) over the heatmap, with the 49 measured probe points drawn as crisp
  crosshairs. Encodes the measured-vs-interpolated truth visually.
- Modes/toggles: deviation-from-mean ↔ absolute color scale; contours on/off; probe points
  on/off. Hover readout: X/Y in mm + Z deviation (bilinear sample).
- Stats panel: range (peak-to-peak), std-dev, highest/lowest (vs mean), mean — computed in
  the browser over all defined cells.
- Validated: rendered against a mock 21×21 mesh in a real browser (correct stats, colorbar,
  geometry; heatmap + crosshairs + contours draw; no JS errors). ~23 KB raw / ~6–7 KB gzipped.

## Validation
- Docker build **both** `--preset coreone` and `--preset coreone_indx` with `-Werror`
  (see [`Firmware-Build-Instructions-Docker.md`](../../Firmware-Build-Instructions-Docker.md)).
- Unit test for the renderer under `tests/unit/lib/WUI/` (existing pattern).
- Manual UI test via **chrome-devtools-mcp / playwright** against a mock `/api/v1/mesh`, then a
  real printer.

## Open questions
- ~~Auth~~ **Resolved:** the route sits inside the `parser.check_auth(out)` gate in
  `prusa_link_api_v1.cpp`, so `/api/v1/mesh` requires the same PrusaLink auth as every other
  `/api/v1` endpoint. The Phase 2 `mesh.html` page must authenticate the same way the SPA does.
- ~~"No mesh available" representation~~ **Resolved:** undefined cells are emitted as JSON
  `null` (NaN in `z_values`), and a top-level `"valid"` boolean reflects `leveling_is_valid()`.
  A never-probed printer yields an all-`null` grid with `"valid":false`.
- Phase 2: whether to add a small link/entry point so users discover `mesh.html` (vs. bookmark).

## Notes
- Honesty about interpolation is the main UX risk: a smooth 21×21 surface looks better than the
  7×7 reality. Marking probed points addresses this.
- Thread safety: individual aligned 32-bit float reads are atomic on Cortex-M4; the snapshot
  under the Marlin lock additionally guards against a partially-updated grid during G29.
