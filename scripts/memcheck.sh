#!/usr/bin/env bash
#
# Local valgrind run via PHP's built-in `run-tests.php -m`. Use sparingly
# (e.g. at sprint boundaries) — full suite under valgrind is slow.
#
# Requires valgrind to be installed system-wide. We do not auto-install.
#
# Usage: scripts/memcheck.sh

set -euo pipefail

if ! command -v valgrind >/dev/null 2>&1; then
    echo "valgrind not installed." >&2
    echo "Install:  sudo apt install valgrind" >&2
    exit 1
fi

cd "$(dirname "$0")/.."

make clean >/dev/null 2>&1 || true
phpize --clean >/dev/null 2>&1 || true
phpize >/dev/null
./configure --enable-numphp >/dev/null

make CFLAGS="-Wall -Wextra -Werror -Wshadow -Wstrict-prototypes -Wmissing-prototypes -O0 -g"

USE_ZEND_ALLOC=0 \
TEST_PHP_ARGS='-m' \
NO_INTERACTION=true \
    make test
