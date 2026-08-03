# Repository setup
* Main branch is called `private`
* We do rebase-merges for the PRs. The user needs to manually squash fixup commits after approving, before merging.
* Code style is documented in CONTRIBUTING.md.

# Review instructions
* Always review commit-by-commit
* Don't report whitespaces/formatting issues. Clangd should take care of that.

# Argo fork development
* For any work on a `*-Argo-stable` branch (new feature, rebase onto a new Prusa
  release, build/validation), read `ARGO-DEVELOPMENT.md` (repo root) FIRST. It holds the
  branch model, the rebase workflow (incl. the "rebase repair — NOT for cherry-picks"
  convention), the feature log, and where to continue.
* Validate firmware changes with the Docker build on BOTH `--preset coreone` and
  `--preset coreone_indx`, with `-Werror` (see `Firmware-Build-Instructions-Docker.md`).
* Per-feature design notes live in `docs/planning/`; user-facing feature docs in `doc/`.
