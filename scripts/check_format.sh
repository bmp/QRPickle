#!/usr/bin/env bash
# Format check for C/C++: only the lines changed since BASE must follow .clang-format, so
# existing code is never mass-reformatted. Files in .clang-format-ignore are skipped.
#   scripts/check_format.sh [BASE]        BASE defaults to the merge-base with origin/main
#   CLANG_FORMAT=/path/to/clang-format    pinned version: pip install clang-format==19.1.7
# Fix locally with: git clang-format --binary "$CLANG_FORMAT" BASE
set -euo pipefail
CLANG_FORMAT=${CLANG_FORMAT:-clang-format}
BASE=${1:-$(git merge-base origin/main HEAD)}
if ! git cat-file -e "${BASE}^{commit}" 2>/dev/null; then
    echo "[check_format] base $BASE not available; nothing to check"; exit 0
fi
out=$(git clang-format --binary "$CLANG_FORMAT" --diff "$BASE" -- 'src/*' 'test/*' 'include/*' 2>&1 || true)
if [[ "$out" == diff* ]]; then
    echo "$out"
    echo "[check_format] FAILED: the changed lines above don't follow .clang-format."
    echo "  Fix with: git clang-format --binary \"$CLANG_FORMAT\" $BASE"
    exit 1
fi
echo "[check_format] OK"
