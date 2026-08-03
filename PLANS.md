# v6.6.3 Argo migration plan

## Objective

Create an untouched stock `v6.6.3` branch from Prusa's official tag and a
`v6.6.3-Argo-stable` branch containing the Argo changes from
`v6.6.2-Argo-stable`, rebased onto that stock release.

## Open questions

- None.

## Approved plan

- Verify Prusa's official `v6.6.3` tag and its relationship to `v6.6.2`.
- Create `v6.6.3` directly at the verified upstream tag without modifying the
  stock tree.
- Replay the `v6.6.2-Argo-stable` commit series onto `v6.6.3`, excluding the
  explicitly non-portable `rebase repair` commit.
- Resolve conflicts commit-by-commit, preserve both upstream behavior and the
  intended Argo features, and create a new repair commit only if 6.6.3-specific
  fixes cannot cleanly belong to their feature commits.
- Update shared Argo documentation and Claude-compatible project memory with
  the new branch status, exact migration result, and validation outcome.
- Build both `coreone` and `coreone_indx` in the documented Docker toolchain
  with `-Werror`.
- Review every replayed commit, compare the old and new feature series, verify
  branch ancestry, and push the requested branches to `origin`.

## Acceptance checks

- `refs/heads/v6.6.3` equals Prusa's verified `refs/tags/v6.6.3` commit.
- `v6.6.3-Argo-stable` descends from the stock `v6.6.3` commit.
- All intended commits from `v6.6.2-Argo-stable` are accounted for and the old
  `rebase repair` commit is absent.
- `git range-diff` and commit-by-commit inspection explain every changed or
  dropped patch.
- Docker builds pass for `coreone` and `coreone_indx` with `-Werror`.
- Shared Markdown identifies `v6.6.3-Argo-stable` as current and gives the next
  agent an accurate continuation point.

## File-level change list

- `ARGO-DEVELOPMENT.md`: add the 6.6.3 branch and migration/validation notes;
  refresh current-branch and feature-log wording.
- `.claude/CLAUDE.md`: retain concise Claude-facing entry-point and validation
  rules, changing it only if reconciliation is needed.
- `PLANS.md`: track implementation status and the cross-agent handoff.
- Active-project Claude memory: reconcile stale migration/build guidance.
- Source files touched by replay conflicts or build repairs: only as required,
  with each change traced to its originating feature commit.

## Risk areas

- Upstream 6.6.3 changes overlap motion, hotend, loadcell, nozzle-cleaner, and
  INDX code, so automatic three-way merges may be semantically wrong even when
  conflict-free.
- Dropping the old repair commit may expose API or build fixes that must be
  re-derived for the new base.
- A local branch and tag share the `v6.6.3` name; verification commands must use
  fully qualified refs to avoid ambiguity.
- Firmware is hardware-facing. No flashing is included, and build artifacts are
  not treated as hardware validation.

## Rollback

- The existing `v6.6.2-Argo-stable` branch remains unchanged.
- Before publication, newly created local branches can be abandoned without
  changing existing history.
- After publication, rollback is to continue using the existing 6.6.2 stable
  branch; no force-push or history rewrite of a published branch is planned.

## Implementation status

- [x] Repository guidance and existing branch history inspected.
- [x] Official tag fetched and verified.
- [x] Stock branch created.
- [x] Argo commits replayed and conflicts resolved.
- [x] Shared documentation and Claude memory updated.
- [x] `coreone` Docker `-Werror` build passed.
- [x] `coreone_indx` Docker `-Werror` build passed.
- [x] Commit-by-commit and range-diff review completed.
- [ ] Fresh 6.6.3 repair and migration-documentation commits approved and created.
- [ ] Requested branches pushed to `origin`.

## Decisions

- Preserve `v6.6.2-Argo-stable` and create the migration on a new branch.
- Follow `ARGO-DEVELOPMENT.md`: do not replay the old base-specific
  `rebase repair` commit.
- Apply the project-level dual-preset build rule where older notes disagree.
- Do not flash firmware as part of this migration.

## Handoff

- Agent: AI agent
- Date: 2026-07-21
- Completed this session:
  - Verified Prusa's lightweight `v6.6.3` tag at
    `ff6658da442b0f81d0283e3f84a18a0029f140d5` and created the matching untouched
    local `v6.6.3` branch.
  - Rebased the portable Argo series onto that tag as local
    `v6.6.3-Argo-stable`, dropping the old base-specific repair commit.
  - Adapted Adaptive Pressure Advance to upstream's `TimeTicks` interface and
    moved the Argo Z-endstop calibration from the newly occupied `M1987` to
    unused `M1988` while preserving Prusa's heater selftest.
  - Re-derived the required 6.6.3 build/API repairs in five source files.
  - Reviewed the 39-commit replay with `git range-diff`: 37 portable patches are
    identical, two have the documented semantic adaptations, and the old repair
    is absent.
  - Passed Docker GCC 13 `-Werror` builds for both required presets.
  - Reconciled `ARGO-DEVELOPMENT.md` and the active-project Claude memory with
    the current branch and dual-preset validation rule.
- Stopped at:
  - Local `v6.6.3-Argo-stable` at rebased tip `145b5f459`, with the fresh repair
    and documentation changes intentionally uncommitted pending user approval.
- Next step:
  - After approval, create the proposed repair and documentation commits, update
    the repair hash in `ARGO-DEVELOPMENT.md`, then push `v6.6.3` and
    `v6.6.3-Argo-stable` to `origin`.
- Open blockers:
  - User approval of the proposed commit messages and file scopes.
- Decisions made this session:
  - Preserve upstream `M1987` and use `M1988` for Argo Z-endstop calibration.
  - Re-derive, rather than replay, the old repair fixes against 6.6.3.
  - Treat build success as compile validation only; no firmware was flashed.

## Notes

- Use fully qualified refs where branch/tag names overlap.
- `coreone` Argo size: flash 1,294,572 B; RAM 121,108 B; CCMRAM 62,144 B.
- `coreone_indx` Argo size: flash 1,312,816 B; RAM 125,164 B; CCMRAM 62,148 B.
- The stock comparison used archive builds reported as `6.6.3+0`; the Argo
  version string is longer, so flash deltas include a few version-string bytes.
- Keep the untracked user-owned `AGENTS.md` out of all commits.
- Keep this `PLANS.md` uncommitted unless the user explicitly asks to track it.
- Never force-push.
