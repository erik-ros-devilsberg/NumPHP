#!/usr/bin/env bash
#
# Measure line coverage with the corrected per-file gcov filter and write a
# summary to build/coverage/. Single canonical entry point used both locally
# and in CI — keeps build artefacts contained.
#
# Output: coverage/summary.txt              (gcovr text summary)
#         coverage/coverage.txt             (per-file table)
#         exit code 0 on pass, non-zero on threshold failure
#
# Usage: scripts/coverage.sh [--fail-under N]    # default N = 80

set -euo pipefail

THRESHOLD="${1:-}"
if [[ "$THRESHOLD" == "--fail-under" ]]; then
    THRESHOLD="${2:-80}"
elif [[ -z "$THRESHOLD" ]]; then
    THRESHOLD=""    # print only, don't gate
fi

cd "$(dirname "$0")/.."
ROOT="$(pwd)"
# Note: phpize owns build/ (clears it on phpize --clean). Keep our outputs
# in a sibling directory we own.
OUT="$ROOT/coverage"

cleanup() {
    # autoconf's conftest probes leak .gcno/.gcda into the project root when
    # CFLAGS contains --coverage; clean them so a fresh checkout has no stray
    # coverage artefacts. .libs/ contents are gitignored, but we wipe them on
    # entry so successive runs are independent.
    rm -f "$ROOT"/a-conftest.gc{no,da} "$ROOT"/conftest.gc{no,da} 2>/dev/null || true
    rm -f "$ROOT"/.libs/*.gcda 2>/dev/null || true
}
trap cleanup EXIT
cleanup    # also clean before, in case of stale state

# Rebuild from scratch under coverage flags. phpize doesn't support
# out-of-tree build, so this runs in-place — but artefacts are confined
# to .libs/ (object files + .gcda/.gcno) and modules/ (the .so).
make clean >/dev/null 2>&1 || true
phpize --clean >/dev/null 2>&1 || true
phpize >/dev/null
./configure --enable-numphp \
    CFLAGS="--coverage -O0 -g" \
    LDFLAGS="--coverage" >/dev/null

# autoconf's compile probes during ./configure leave a-conftest.gc{no,da}
# in the project root. They reference a long-deleted conftest.c and will
# trip up gcovr later. Wipe them now.
rm -f "$ROOT"/a-conftest.gc{no,da} "$ROOT"/conftest.gc{no,da}

make >/dev/null

# Now safe to create the output directory — phpize is done.
mkdir -p "$OUT"

# Run the full phpt suite with no interactive prompt.
NO_INTERACTION=true make test >/dev/null 2>&1 || {
    echo "tests failed under coverage build" >&2
    exit 1
}

# Filter to every C source actually shipped by config.m4. The previous CI
# filter was just ndarray.c + ops.c; this widens to the real surface.
FILTERS=(
    --filter 'numphp\.c$'
    --filter 'ndarray\.c$'
    --filter 'ops\.c$'
    --filter 'linalg\.c$'
    --filter 'io\.c$'
    --filter 'bufferview\.c$'
    --filter 'nditer\.c$'
)

GCOVR_OPTS=(
    -r .
    "${FILTERS[@]}"
    --exclude '.*conftest.*'
    --gcov-ignore-errors=no_working_dir_found
)

gcovr "${GCOVR_OPTS[@]}" --print-summary > "$OUT/summary.txt"
gcovr "${GCOVR_OPTS[@]}"                  > "$OUT/coverage.txt"

echo "--- coverage report ---"
cat "$OUT/coverage.txt"

if [[ -n "$THRESHOLD" ]]; then
    LINE_PCT=$(grep -oP 'lines: \K[0-9.]+' "$OUT/summary.txt" | head -1)
    LINE_PCT=${LINE_PCT%.*}
    if (( LINE_PCT < THRESHOLD )); then
        echo "FAIL: line coverage $LINE_PCT% < threshold $THRESHOLD%" >&2
        exit 1
    fi
    echo "PASS: line coverage $LINE_PCT% >= threshold $THRESHOLD%"
fi
