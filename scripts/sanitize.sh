#!/usr/bin/env bash
#
# Local ASan + UBSan run. Mirrors the `sanitizers` CI job.
#
# Catches: heap/stack overflows, use-after-free, signed integer overflow,
# alignment violations, and other UB. Loud on the first hit.
#
# Leak detection is ON. PHP-internal startup leaks (getaddrinfo, dlopen
# _init, module table) are suppressed via lsan.supp at the repo root.
# Any leak whose stack contains a numphp_* / zim_NDArray_* /
# zim_Linalg_* / zim_BufferView_* frame is a real bug and will fail
# this script.
#
# Usage: scripts/sanitize.sh

set -euo pipefail

cd "$(dirname "$0")/.."

LIBASAN="$(gcc -print-file-name=libasan.so)"
LIBUBSAN="$(gcc -print-file-name=libubsan.so)"

if [[ ! -e "$LIBASAN" ]]; then
    echo "libasan not found via gcc -print-file-name. Is gcc with sanitizer support installed?" >&2
    exit 1
fi

make clean >/dev/null 2>&1 || true
phpize --clean >/dev/null 2>&1 || true
phpize >/dev/null
./configure --enable-numphp >/dev/null

make CFLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g -Wall -Wextra -Werror -Wshadow -Wstrict-prototypes -Wmissing-prototypes" \
     LDFLAGS="-fsanitize=address,undefined"

# LD_PRELOAD ensures ASan loads before libc malloc — the extension is
# dlopen'd into PHP, so without preload ASan complains
# "ASan runtime does not come first in initial library list".
LD_PRELOAD="$LIBASAN:$LIBUBSAN" \
USE_ZEND_ALLOC=0 \
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:halt_on_error=1 \
LSAN_OPTIONS=suppressions=$(pwd)/lsan.supp \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
NO_INTERACTION=true \
    make test
