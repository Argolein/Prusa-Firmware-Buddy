# PLANS.md

## Objective
Fix CORE One 1.5GT X/Y steps-per-mm handling so homing and selftest axis calibration use consistent logical motion parameters when X/Y differs from stock 100.

## Open questions
- none

## Approved plan
- Port the relevant PR #5062 fixes locally without changing unrelated behavior.
- Preserve the existing junction-deviation handling while extending temporary motion-parameter save/load to include axis steps.
- Update phase-stepping motor parameters whenever planner positioning is refreshed.
- Use logical planner axis positions for selftest axis length measurement instead of raw stepper motor positions, which are A/B motor positions on CoreXY.
- Build COREONE with the known-good Docker/GCC13 toolchain and `-Werror`.

## Implementation status
- [ ] Not started
- [ ] In progress
- [x] Done

## Decisions
- Use a local, minimal port of PR #5062 instead of applying the full patch verbatim, because this branch keeps `junction_deviation_mm` separate from `planner_settings_t`.
- Treat the observed Y homing calibration failure with X/Y = 101.5873 as the concrete symptom of the scaling/cache inconsistency fixed by PR #5062.
- Treat the remaining Y217 selftest failure as a CoreXY logical-vs-raw-axis measurement bug: raw `stepper.position(Y_AXIS)` is motor B on CoreXY, while the selftest acceptance range is in logical Y millimeters.

## Handoff
- Agent: Codex
- Date: 2026-04-26
- Completed this session:
  - Identified PR #5062 as matching the 1.5GT/M92/Core One scaling and calibration bug.
  - Ported the motion-parameter and phase-stepping consistency fixes locally.
  - Changed selftest axis length measurement to use logical planner axis positions for CoreXY correctness.
  - Built COREONE with Docker/GCC13, bootloader enabled, and -Werror.
- Stopped at:
  - Build artifact generated in build/products-docker-gcc13-pr5062-selftest-logical-axis/.
- Next step:
  - Flash coreone_release_boot.bbf and run Y homing calibration on the printer.
- Open blockers:
  - none
- Decisions made this session:
  - Use a local minimal port preserving branch-specific junction-deviation handling.
  - Measure selftest axis travel in logical mm through Planner instead of raw motor steps.

## Notes
- Build with the Docker/GCC13 workflow documented in README.md.
