# Argo fork — development guide

Start-here notes for working on the **`*-Argo-stable`** branches of this Prusa-Buddy fork.
This is the durable entry point: branch model, the rebase workflow, what has been added on
top of stock Prusa firmware, and where to continue. Per-feature design records live in
[`docs/planning/`](docs/planning/); user-facing feature docs live in [`doc/`](doc/).

---

## Branch model

| Branch | Meaning |
|--------|---------|
| `v6.5.7` | Stock Prusa release (upstream, untouched) |
| `v6.5.7-Argo-stable` | Argo features on top of `v6.5.7` (last known-good before the 6.6.0 bump) |
| `v6.6.0` | Stock Prusa release (upstream, untouched) |
| `v6.6.0-Argo-stable` | **Current working branch** — Argo features rebased onto `v6.6.0` |

The `*-Argo-stable` branches carry the same set of personal features, re-based onto each new
stock Prusa release.

---

## Rebase workflow (read before rebasing onto a new Prusa release)

When Prusa publishes the next release (e.g. `v6.7.0`), the goal is to re-base the **clean
feature commits** onto it — **not** to carry forward an old `*-Argo-stable` branch with its
repair commit.

### The "rebase repair" convention

A rebase onto a new base can break feature commits in ways that are **specific to that base**
(APIs the feature calls may have changed, stale code may resurface from a bad 3-way merge,
stricter `-Werror`, etc.). Those fixes are collected into a single commit whose subject begins:

> `rebase repair — NOT intended for future cherry-picks / rebases`

(example on this branch: `f8f75a630`). Its body lists each fix and the original feature commit
it really belongs to.

**On the next rebase: DROP the repair commit and re-derive the equivalent fixes against the new
base** (the correct fix may differ). Do not blindly replay it.

### Recommended steps for the next release

1. Fetch the new stock tag/branch from Prusa (`upstream`), e.g. `v6.7.0`.
2. Rebase the **feature commits only** (the list under "Feature log" below, minus any
   `rebase repair` commit) onto `v6.7.0`. Easiest: branch the clean feature set from
   `v6.5.7-Argo-stable` or cherry-pick the feature commits; do **not** start from
   `v6.6.0-Argo-stable`'s repair commit.
3. Build `coreone` and `coreone_indx` with `-Werror` (see "Build"). Fix what breaks.
4. Collect those fixes into a fresh `rebase repair — NOT intended ...` commit, with a body
   mapping each fix to its origin feature commit (same format as `f8f75a630`).
5. Tag the result `v6.7.0-Argo-stable`.

> Even cleaner (optional, history rewrite): instead of a separate repair commit, fold each
> repair hunk back into its origin feature commit via interactive rebase, so the feature
> branch stays self-consistent. Only do this on already-shared commits deliberately.

---

## Build

See [`Firmware-Build-Instructions-Docker.md`](Firmware-Build-Instructions-Docker.md). Builds
run in the `prusa-buddy-build:gcc13` Docker image to match Prusa's toolchain.

- Validate features on `--preset coreone`. (Previously both `coreone` and `coreone_indx` were
  built, but the maintainer doesn't use INDX, so `coreone` alone is sufficient as of
  2026-06-27. Build `coreone_indx` only if a change specifically touches INDX-only paths.)
- `-DCUSTOM_COMPILE_OPTIONS:STRING="-Werror"` is the intended strictness. If a fresh rebase
  has pre-existing `-Werror` debt unrelated to your change, that belongs in the rebase-repair
  commit — don't silently drop `-Werror`.

---

## Feature log (Argo additions on top of stock `v6.6.0`)

Oldest → newest. Commit hashes are for this `v6.6.0-Argo-stable` rebase and will change on the
next rebase.

| Area | Feature | Commit(s) | Notes |
|------|---------|-----------|-------|
| Mechanics | 1.5GT belt support + default steps/mm | `1b087b650`, `f0dedbd69`, `890721f20` | `DEFAULT_AXIS_STEPS_PER_UNIT`, "steps/mm" setting |
| Mechanics | Custom motor current | `24a2d6d43` | |
| Mechanics | Increased XY/Z park speed | `0fa6b1789` | |
| Chamber | Higher max chamber temp + safety margins | `a9edda91a`, `458ace999` | up to 65 °C |
| Motion | Adaptive Pressure Advance (no planner flush on `M572 S`) | `e1e470b7d` | design: [`docs/planning/adaptive-pressure-advance.md`](docs/planning/adaptive-pressure-advance.md) |
| Motion | CoreXY selftest axis-length calibration fix | `a10f4b38e` | adds `phase_stepping::update_axis_motor_params` |
| Homing | Automatic Z-alignment during `G28` | `3cb8c663e` | |
| Network | Wi-Fi / Ethernet mutually exclusive at runtime | `6e92d4af5`, `176caf4ca` | |
| Filament | Toggle "Preheat & ram before unload" (Advanced Settings): OFF = cold unload, skips BOTH preheat and ramming | `106cdb7f9` (+ ramming skip) | ramming skipped in `ram_sequence_process` because Prusa 6.6.0 rams even when cold |
| Filament | Cool down nozzle after load when idle | `3a37b1eb8` | |
| Filament | **Filament Color Manager** (per-tool color, autoload prompt, PrusaLink) | `25780ffad` | design: [`docs/planning/filament-color-manager.md`](docs/planning/filament-color-manager.md); user docs: [`doc/filament_color_manager.md`](doc/filament_color_manager.md) |
| Network | **Bed Mesh Viewer** (`GET/POST /api/v1/mesh` + embedded `mesh.html` heatmap with "Run bed leveling") | `6f13a0986`, `37abeb50e`, `089732d65`, `0037a6479` | design: [`docs/planning/bed-mesh-viewer.md`](docs/planning/bed-mesh-viewer.md); served at `http://<printer>/mesh.html` |
| Network | **PrusaLink Chamber Temperature** (Dashboard sidebar row below Heatbed) | _(this commit)_ | `temp_chamber`/`target_chamber` in `/api/v1/status` (`HAS_CHAMBER_API()` guard) + web bundle telemetry map + `index.html` row; design: [`docs/planning/prusalink-chamber-temperature.md`](docs/planning/prusalink-chamber-temperature.md) |
| — | **Rebase repair (drop on next rebase)** | `f8f75a630` | see "Rebase workflow" |

---

## Open / future work

- **Prusa Link web modernization** — [`PLANS-Web.md`](PLANS-Web.md): Bed Mesh API + viewer
  **done** (`6f13a0986`, `37abeb50e`); G-code console etc. still open. The Filament Color
  Manager's `GET/PUT /api/v1/filament` endpoints
  (`lib/WUI/nhttp/filament_renderer.*`, `filament_command.*`, routed in
  `prusa_link_api_v1.cpp`) are a working template for that plan's "Phase 1" backend endpoints
  (segmented JSON renderer + body-parsing PUT handler + `handler.h` variant registration +
  explicit `SendJson<>` instantiation in `send_json.cpp`).

---

## Conventions / where to continue

- Per-feature **design records** → `docs/planning/` (archived when done, with a status banner).
- Per-feature **user docs** → `doc/`.
- Keep stock Prusa files (`README.md`, `LICENSE.md`, upstream sources) untouched where possible
  so rebases stay clean.
- Commit messages follow the repo's conventional style (`feat(...)`, `fix(...)`); the
  rebase-repair commit is the deliberate exception (see above).
