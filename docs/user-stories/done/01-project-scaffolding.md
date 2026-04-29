# Story 1: Project Scaffolding & Architecture Decisions

> Part of [Epic: NumPHP](epic-numphp.md)

**Outcome:** Buildable extension skeleton, cross-cutting decisions locked in writing, testing infrastructure runs in CI.

## Toolchain required
- `php8.x-dev` (gives `phpize` and headers)
- `gcc` / `clang`
- `autoconf`, `make`
- `libopenblas-dev`
- `liblapack-dev` (separate package on most distros even when OpenBLAS is installed)
- `valgrind`, `clang` with `-fsanitize=address,undefined` for CI checks

## Directory structure
```
numphp/
├── config.m4
├── numphp.c
├── numphp.h
├── ndarray.c/.h
├── ops.c/.h
├── linalg.c/.h
├── nditer.c/.h          // shared iterator, lands in Story 7, header stub here
├── tests/
│   ├── 001-load-extension.phpt
│   └── ...
└── CHANGELOG.md
```

`package.xml` is introduced in Story 12, not in v0.

## Build cycle
```bash
phpize
./configure
make
make test                                 # runs phpt files
USE_ZEND_ALLOC=0 TEST_PHP_ARGS="-m" make test   # under valgrind
make install
```

## config.m4

```m4
PHP_ARG_ENABLE(numphp, enable numphp, [ --enable-numphp Enable numphp])

if test "$PHP_NUMPHP" = "yes"; then
  PHP_CHECK_LIBRARY(openblas, cblas_dgemm,
    [PHP_ADD_LIBRARY(openblas,, NUMPHP_SHARED_LIBADD)],
    [AC_MSG_ERROR([OpenBLAS not found])])

  PHP_CHECK_LIBRARY(lapack, dgetri_,
    [PHP_ADD_LIBRARY(lapack,, NUMPHP_SHARED_LIBADD)],
    [AC_MSG_ERROR([LAPACK not found])])

  PHP_NEW_EXTENSION(numphp,
    numphp.c ndarray.c ops.c linalg.c nditer.c,
    $ext_shared)
fi
```

## Decisions to lock in writing

### PHP target
**PHP 8.2+, NTS only for v1.** ZTS deferred — no per-request globals are added without a thread-safety review.

### dtype scope
v1 dtypes: `float32`, `float64`, `int32`, `int64`. Booleans, complex numbers, float16 deferred.

### dtype promotion (binary ops)

| op1 \ op2 | f32 | f64 | i32 | i64 |
|-----------|-----|-----|-----|-----|
| **f32**   | f32 | f64 | f32 | f64 |
| **f64**   | f64 | f64 | f64 | f64 |
| **i32**   | f32 | f64 | i32 | i64 |
| **i64**   | f64 | f64 | i64 | i64 |

Rule: int+int → wider int; int+float → float of at least equal precision; mixed int/float widths default to `f64`. Matches NumPy.

### Exception hierarchy

```
\NDArrayException          (base, extends \RuntimeException)
├── \ShapeException        shape / dimension mismatch
├── \DTypeException        unsupported or incompatible dtype
└── \IndexException        out of bounds, invalid slice
```

Registered in `MINIT`. All `zend_throw_exception` calls in subsequent stories target the appropriate subclass.

### Memory ownership
- Refcounted underlying data buffer (defined in Story 2), separate from the `numphp_ndarray` metadata shell.
- Views never free the buffer; they hold a refcount on it. Owners decrement on free.
- No copy-on-write in v1 — explicit `copy()` is the user's escape hatch.

### Contiguity tracking
`flags` bitfield on `numphp_ndarray`: `C_CONTIGUOUS`, `F_CONTIGUOUS`, `WRITEABLE`. Recomputed in any op that produces a view; preserved across no-copy ops.

### Error policy
- Float division by zero → IEEE 754 (`inf`, `nan`), no exception.
- Int division by zero → `\DivisionByZeroError`.
- Out-of-bounds index → `\IndexException`.
- Ragged `fromArray` input → `\ShapeException`.

## Testing infrastructure
- phpt files in `tests/`, one per surface (load, ndarray construction, ops, errors, edge cases).
- CI matrix: PHP 8.2 / 8.3 / 8.4, Linux + macOS, debug build under valgrind, release build under ASAN/UBSAN.
- Coverage via `gcov`; fail CI under 80% on `ndarray.c` and `ops.c`.

## CI
GitHub Actions runs the full build cycle plus valgrind and sanitizer matrices on every PR.
