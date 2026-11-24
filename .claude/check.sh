#!/bin/bash
# Verification gate — change-scoped on ~/.claude/scripts/gate/scope.sh.
# All checks run in the Docker build image (Firmware-Build-Instructions-Docker.md);
# the host lacks the unit-test dependencies. A named volume keeps the build
# folders between runs (one volume per checkout, so parallel worktrees do not
# share one), so a run rebuilds only what changed.
set -euo pipefail
cd "$(dirname "$0")/.."
. "$HOME/.claude/scripts/gate/scope.sh"
scope_init
scope_skip_if_unchanged

run_unit=0
run_nhttp=0
run_fw=0
if scope_changed 'lib/Marlin/Marlin/src/feature/phase_stepping/*' 'tests/unit/lib/Marlin/phase_stepping/*' 'tests/stubs/Marlin/src/feature/phase_stepping/*'; then
  run_unit=1
fi
# The phase stepping renderer test also compiles against the phase stepping
# sources and stubs, and handler.h pulls in filament.hpp and the uuid headers.
if scope_changed 'lib/WUI/*' 'tests/unit/lib/WUI/*' 'lib/Marlin/Marlin/src/feature/phase_stepping/*' \
  'tests/stubs/Marlin/src/*' 'src/common/filament.hpp' 'src/module/utils/uuid/*'; then
  run_nhttp=1
fi
if scope_changed 'src/*' 'lib/*' 'include/*' 'cmake/*' 'CMakeLists.txt' 'CMakePresets.json' 'utils/build.py' \
  'ProjectOptions.cmake' 'version.txt'; then
  run_fw=1
fi

if [ "$run_unit$run_nhttp$run_fw" != "000" ]; then
  echo "== docker: phase stepping unit tests=$run_unit, nhttp unit tests=$run_nhttp, coreone_indx -Werror build=$run_fw =="
  mkdir -p build/products-docker-gcc13-coreone_indx
  # git inside the container needs the main .git at the path the worktree's .git file names.
  git_common=$(cd "$(git rev-parse --git-common-dir)" && pwd)
  git_named=$git_common
  if [ -f .git ]; then
    git_named=$(sed -n 's/^gitdir: //p' .git)
    git_named=${git_named%/worktrees/*}
  fi
  docker run --rm --platform linux/amd64 \
    -e RUN_UNIT="$run_unit" -e RUN_NHTTP="$run_nhttp" -e RUN_FW="$run_fw" \
    -v "$PWD":/host:ro \
    -v "$git_common":"$git_named":ro \
    -v "$PWD/build/products-docker-gcc13-coreone_indx":/out \
    -v "argo-gate-$(printf %s "$PWD" | shasum | cut -c1-12)":/cache \
    prusa-buddy-build:gcc13-v8 \
    bash -lc '
set -euo pipefail
git config --global --add safe.directory "*"
mkdir -p /cache/src
tar -C /host --exclude="./.dependencies" --exclude="./.venv" --exclude="./build" \
  --exclude="./build-*" -cf - . | tar -C /cache/src -xf -
cd /cache/src
# Drop files the checkout no longer has, so a deleted header cannot build green from the cache.
(cd /host && find . -path ./.dependencies -prune -o -path ./.venv -prune -o -path ./build -prune \
  -o -path "./build-*" -prune -o -type f -print | sort) > /tmp/host.lst
find . -path ./.dependencies -prune -o -path ./.venv -prune -o -path ./cmake/GccArmNoneEabi.cmake -prune \
  -o -type f -print | sort > /tmp/cache.lst
comm -13 /tmp/host.lst /tmp/cache.lst | tr "\n" "\0" | xargs -0 -r rm -f
ln -sfn /work/.dependencies .dependencies
ln -sfn /work/.venv .venv
git show refs/tags/v6.5.3:cmake/GccArmNoneEabi.cmake > cmake/GccArmNoneEabi.cmake
. .venv/bin/activate
if [ "$RUN_UNIT" = 1 ]; then
  python3 utils/build_tests.py --build-dir /cache/build-tests Marlin_phase_stepping_tests
  /cache/build-tests/tests/unit/lib/Marlin/phase_stepping/Marlin_phase_stepping_tests
fi
if [ "$RUN_NHTTP" = 1 ]; then
  python3 utils/build_tests.py --build-dir /cache/build-tests nhttp_tests
  /cache/build-tests/tests/unit/lib/WUI/nhttp/nhttp_tests
fi
if [ "$RUN_FW" = 1 ]; then
  python3 utils/build.py --preset coreone_indx --bootloader yes \
    --build-dir /cache/build-coreone_indx --products-dir /out --skip-bootstrap \
    -DCUSTOM_COMPILE_OPTIONS:STRING="-Werror"
fi
'
fi

scope_uncovered
scope_green
