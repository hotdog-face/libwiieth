#!/bin/bash
# Builds libwiieth and the examples in the devkitpro/devkitppc Docker image (no local devkitPro needed).
#   ./build.sh                 make all examples, here (Docker / Colima)
#   ./build.sh clean all       any make targets
#   BUILD_HOST=user@box ./build.sh   on another machine with Docker (rsync + ssh), results copied back
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
IMAGE="${IMAGE:-devkitpro/devkitppc:latest}"
TARGETS=("$@")
[ ${#TARGETS[@]} -eq 0 ] && TARGETS=(all examples)
if [ -z "${BUILD_HOST:-}" ]; then
    exec docker run --rm -u "$(id -u):$(id -g)" -v "$HERE":/src -w /src "$IMAGE" make "${TARGETS[@]}"
fi
rsync -a --delete --exclude .git "$HERE/" "$BUILD_HOST:libwiieth-build/"
ssh "$BUILD_HOST" "cd ~/libwiieth-build && docker run --rm -u \$(id -u):\$(id -g) -v \$PWD:/src -w /src $IMAGE make ${TARGETS[*]}"
rsync -a --include '*/' --include '*.a' --include '*.dol' --exclude '*' "$BUILD_HOST:libwiieth-build/" "$HERE/"
