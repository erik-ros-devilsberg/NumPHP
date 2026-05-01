---
story: Project Layout — Move sources into src/
created: 2026-05-01
---

> Part of [Epic: NumPHP](epic-numphp.md)

## Description

The project root currently has 20 tracked files including 15 C/H source
files (`numphp.c/h`, `ndarray.c/h`, `ops.c/h`, `linalg.c/h`, `io.c/h`,
`bufferview.c/h`, `nditer.c/h`, plus `lapack_names.h`). Mixed in with
those are `README.md`, `CHANGELOG.md`, `CLAUDE.md`, `.gitignore`, and
`config.m4`. Visually the source files dominate the root and crowd out
the meta files newcomers actually want to read first.

Move the C sources into `src/` (modern PHP-extension layout — used by
Swoole, recent phpredis branches). PECL still works because
`package.xml` lists files by relative path. The root drops to
~6 tracked files, all of them things a newcomer should see first
(`README.md`, `CHANGELOG.md`, `LICENSE`, `CLAUDE.md`, `.gitignore`,
`config.m4`).

PECL convention is flat layout (most ext/ in php-src); we accept the
small departure for a tidier repo.

## Acceptance Criteria

- All `.c` and `.h` files (including `lapack_names.h`) live under `src/`.
- `config.m4` references the new paths via `PHP_NEW_EXTENSION(numphp,
  src/numphp.c src/ndarray.c …)` and `PHP_ADD_INCLUDE([…/src])` so
  `#include "numphp.h"` still resolves from any source file.
- `phpize && ./configure && make` succeeds without manual intervention,
  produces a working `modules/numphp.so`.
- All 60 phpt tests pass + 1 FFI skip preserved; build clean at
  `-Wall -Wextra`.
- CI yaml, `scripts/coverage.sh`, and the snippet harness work without
  changes (or with mechanical path updates only — no logic changes).
- `LICENSE` file added at the repo root (we ship without one today;
  README acknowledges it's TBD).
- `PHP_NUMPHP_VERSION` bumped (e.g. to `0.0.12`).
- `CHANGELOG.md` entry under a new release header.

## Out of scope

- `package.xml` for PECL distribution — that's Story 12.
- Any source-file split, rename, or refactor inside `src/`. Files move,
  contents do not.
- Build-system overhaul (autotools → CMake, etc.). Out.
- `tests/`, `examples/`, `docs/`, `scripts/`, `.github/` — already
  organised, untouched.

## Notes

The choice between flat root (PECL-canonical) and `src/` (modern) was
considered. Flat is what `ext_skel.php` produces and what most
ext/ in php-src does. `src/` is cleaner for a repo with 7+ C
source files and is increasingly common. Picked `src/` because:

1. The "meta files" at the root (README, CHANGELOG, etc.) are what a
   newcomer reads first; they currently compete with 15 source files
   for visual attention.
2. Future sprints will add more C files (potential `random.c`,
   `complex.c`, etc.); flat root degrades further with each addition.
3. PECL still works — `package.xml` accepts paths.

Locked as decision 28 in `docs/system.md` after the move.
