#!/usr/bin/env bash
# Run QRPickle's checks in the development container (same tools and versions as CI).
#   scripts/dev.sh [--ubuntu 24.04|26.04] <command>
# Commands:
#   test     native unit tests            check   cppcheck (no defects allowed)
#   build    firmware + filesystem image  lint    ruff + clang-format on changed lines
#   manual   PDF manual -> release/       pages   Pages site (OTA + installer) -> release/site
#   shell    interactive shell            all     everything above except shell
# Needs podman or docker. The image is built on first use (and when Containerfile changes).
set -euo pipefail
cd "$(dirname "$0")/.."

UBUNTU=26.04
if [[ "${1:-}" == "--ubuntu" ]]; then UBUNTU=$2; shift 2; fi
CMD=${1:-all}

ENGINE=$(command -v podman || command -v docker || true)
[[ -n "$ENGINE" ]] || { echo "dev.sh: podman or docker is required" >&2; exit 2; }

IMAGE="qrpickle-dev:${UBUNTU}"
HASH=$(sha256sum Containerfile | cut -c1-12)
if [[ "$("$ENGINE" image inspect --format '{{ index .Config.Labels "containerfile" }}' "$IMAGE" 2>/dev/null)" != "$HASH" ]]; then
    echo "dev.sh: building $IMAGE"
    "$ENGINE" build -q --build-arg "UBUNTU=$UBUNTU" --label "containerfile=$HASH" -t "$IMAGE" -f Containerfile . >/dev/null
fi

VERSION=$(grep FW_VERSION src/core/metadata.h | cut -d'"' -f2)
# The source is mounted with :z (shared SELinux label): with :Z every run gets a private label,
# and files copied into the cache volume (e.g. data_gz) become unreadable to the next run.
run() {
    "$ENGINE" run --rm -i $([[ -t 0 ]] && echo -t) \
        -v "$PWD:/src:z" \
        -v "qrpickle-workspace-$UBUNTU:/pio-workspace" -v "qrpickle-core-$UBUNTU:/pio-core" \
        "$IMAGE" bash -euo pipefail -c "$1"
}

steps_test='pio test -e native'
steps_check='pio check -e cyd --severity=high --severity=medium --fail-on-defect=medium'
steps_build='pio run -e cyd && pio run -e cyd -t buildfs'
steps_lint="ruff check && scripts/check_format.sh && python3 scripts/release_notes.py $VERSION >/dev/null"
steps_manual="mkdir -p release && python3 scripts/build_manual.py --version $VERSION --out release/QRPickle_Documentation_${VERSION}-container.pdf"
steps_pages="pio run -e cyd -t buildfs >/dev/null && python3 scripts/release_notes.py $VERSION > /tmp/n.txt && \
  python3 scripts/make_pages_site.py --build-dir /pio-workspace/build/cyd --out release/site --version $VERSION --notes-file /tmp/n.txt"

case "$CMD" in
    test)   run "$steps_test" ;;
    check)  run "$steps_check" ;;
    build)  run "$steps_build" ;;
    lint)   run "$steps_lint" ;;
    manual) run "$steps_manual" ;;
    pages)  run "$steps_build >/dev/null && $steps_pages" ;;
    shell)  run "bash" ;;
    all)    run "$steps_test && $steps_check && $steps_build && $steps_lint && $steps_manual && $steps_pages && echo 'dev.sh: all checks passed'" ;;
    *)      sed -n '2,10p' "$0"; exit 2 ;;
esac
