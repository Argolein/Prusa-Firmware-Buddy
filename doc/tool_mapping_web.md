# Tool mapping over the web (PrusaLink)

Brings the printer's on-screen **Tools Mapping** step to the PrusaLink web page, so you can
start a multi-material print from OrcaSlicer (or any PrusaLink client) and finish the
filament → tool assignment **in your browser** instead of walking to the printer.

It reuses the exact same firmware flow the touchscreen uses — the print pauses at the normal
"tools mapping" preview step and the web page just drives it remotely.

## What you get

When you upload-and-print a G-code that needs tool mapping (an MMU or INDX / multi-tool print),
the print no longer auto-starts. Instead it **holds at the tools-mapping step**, and the
PrusaLink page shows an overlay:

- **G-code filaments** on the left — each with its index, color swatch and type from the sliced file.
- A **tool dropdown** on the right for each filament — listing your printer's tools with their
  loaded type and Color-Manager color.
- A **⚠** appears if you map a filament onto a tool loaded with a different type (non-blocking).
- An optional **Spool join** section — "when Tool N runs out, continue on Tool M".
- **Print** applies the mapping and starts; **Back** aborts.

The printer's touchscreen shows the same screen at the same time — either one can drive it.

## Using it from OrcaSlicer

1. In OrcaSlicer, set the printer's **Device-UI URL** to your printer's PrusaLink page, e.g.
   `http://<printer-ip>/#print`.
2. Slice, then **upload and print** via PrusaLink as usual.
3. Open the Device tab (the PrusaLink page). The mapping overlay appears once the printer is
   holding at the tools-mapping step. Assign each filament to a tool and press **Print**.

You must be signed in to PrusaLink (the same as any other control on the page).

## Notes & limitations

- **One tool per filament.** The firmware tool mapper is a strict bijection — two G-code
  filaments can't be assigned to the *same* tool. Use **Spool join** to chain tools instead.
  The overlay blocks Print (with a message) if you pick the same tool twice.
- **MMU / INDX only for the overlay.** The mapping overlay only appears when tool mapping is
  actually possible (MMU enabled, or an INDX toolchanger). A plain single-tool Core One never
  shows it — there is nothing to map with one tool.
- **Single-tool multi-material prints** (a bare Core One, no MMU): a multi-filament G-code now
  prints **mono-color** on the single tool instead of failing — every tool change resolves to
  the one tool. (Previously this fatal-errored on the first `T1`.) The color/tool the model was
  designed for is ignored; it prints with whatever filament is loaded.

## Under the hood

- Endpoints: `GET /api/v1/mapping` (current filaments + tools + mapping),
  `PUT /api/v1/mapping` (apply a mapping + spool join), `POST /api/v1/mapping/confirm` and
  `.../cancel` (Print / Abort the preview step). Only active while the print-preview FSM is
  holding at the tools-mapping phase.
- The web start-path holds at mapping via `PreviewSkipIfAble::preview` (web-started prints only;
  the touchscreen / USB / Prusa Connect start-paths are unchanged).
- Design record: [`docs/planning/prusalink-tools-mapping.md`](../docs/planning/prusalink-tools-mapping.md).
