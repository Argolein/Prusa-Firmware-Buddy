Prusa Firmware Buddy Instructions

Build:

cd /Users/ArgoMac/GitHub-Development/Prusa-Firmware-Buddy

# Since 8.0.1 the image builds the ESP Wi-Fi SDKs, whose compilers exist only
# for x86-64 Linux: build and run it as linux/amd64 on Apple Silicon.
docker build --platform linux/amd64 \
  -f utils/holly/Dockerfile \
  -t prusa-buddy-build:gcc13-v8 .

mkdir -p build/products-docker-gcc13-coreone

# Building from a worktree (.claude/worktrees/...): its .git file points at the
# main repo's .git, which the container cannot see. git then fails, the build
# number falls back to 0 (8.0.1+0) and the printer skips the file as "not
# newer". Add this mount and run
# `git config --global --add safe.directory "*"` first inside the container:
#   -v "$(git rev-parse --git-common-dir)":"$(git rev-parse --git-common-dir)":ro
docker run --rm --platform linux/amd64 \
  -v "$PWD":/host:ro \
  -v "$PWD/build/products-docker-gcc13-coreone":/out \
  prusa-buddy-build:gcc13-v8 \
  bash -lc '
set -euo pipefail

mkdir -p /work/src
tar -C /host \
  --exclude="./.dependencies" \
  --exclude="./.venv" \
  --exclude="./build" \
  --exclude="./build-*" \
  -cf - . | tar -C /work/src -xf -

cd /work/src
ln -sfn /work/.dependencies .dependencies
ln -sfn /work/.venv .venv

git show refs/tags/v6.5.3:cmake/GccArmNoneEabi.cmake \
  > cmake/GccArmNoneEabi.cmake

. .venv/bin/activate

python3 utils/build.py \
  --preset coreone \
  --bootloader yes \
  --build-dir /work/build-coreone-docker-gcc13 \
  --products-dir /out \
  --skip-bootstrap \
  -DCUSTOM_COMPILE_OPTIONS:STRING="-Werror"
'
