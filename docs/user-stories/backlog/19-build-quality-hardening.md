---
story: Build Quality Hardening
created: 2026-05-03
---

## Description

Tighten the C build to catch regressions earlier and surface
real bugs that the current `-Wall -Wextra` build misses. The
project is at 0.0.18 with a green build and 67/67 phpt; this
story locks that quality in and adds new diagnostic coverage
without paying for noise from Zend/PHP headers.

Scope is the C extension build only (CFLAGS in `config.m4` /
`Makefile.fragments` / CI). No source-level rewrites beyond
fixing whatever the new flags surface.

### Compiler flags to add

- **`-Werror`** — promote warnings to errors. Build is currently
  warning-free at `-Wall -Wextra`, so this is a no-op today and
  prevents regressions tomorrow. Cheap win.
- **`-Wshadow`** — flags shadowed locals/parameters. Real-bug
  finder; common source of subtle errors in long functions like
  `numphp_compare` / reduction kernels.
- **`-Wstrict-prototypes`** — bans `()` argument lists (which
  mean "any args" in C) in favour of `(void)`. Catches function
  declarations that look correct but aren't.
- **`-Wmissing-prototypes`** — every non-static function must
  have a prior prototype. Forces internal helpers to be `static`
  or properly declared in a header. Cleans up linkage hygiene.
- **`-Wcast-align`** — flags pointer casts that increase
  alignment requirements. Relevant given the buffer-arithmetic
  in `nditer.c` and `ops.c`.
- **`-Wnull-dereference`** — flow-sensitive null deref detection.
  Worth its keep.
- **`-Wdouble-promotion`** — flags implicit `float → double`
  promotions. Not a correctness flag per se, but exposes places
  where f32 arithmetic accidentally widens to f64; relevant for
  the f32 fast paths.

### Compiler flags explicitly NOT adopted

- **`-Wpedantic`** — Zend macros (`PHP_FUNCTION`, arginfo,
  method tables) expand to non-ISO-C constructs. Hundreds of
  warnings from headers we don't own. Suppressing per-include
  via `#pragma GCC diagnostic` is more mess than it's worth on
  a PHP extension.
- **`-Wconversion`** — too noisy for numerical code (every
  `size_t ↔ int` and integer narrowing fires). Revisit only if
  we get serious about a strict-typed pass.

### CI / toolchain

- Apply the new flags to the local build (`make CFLAGS="..."`)
  AND to CI so regressions are caught on PRs.
- Keep `-O2 -g` as today.
- Verify the build stays green on the gcc that CI uses; if a
  flag fires on Zend headers (esp. `-Wmissing-prototypes` or
  `-Wcast-align`), wrap that include with
  `#pragma GCC diagnostic push/ignored "-Wxxx"/pop` rather
  than dropping the flag.

### Sanitizers — ASan + UBSan

Add a new CI job that builds with
`-fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g`
(both CFLAGS and LDFLAGS) and runs the full phpt suite +
doc-snippet harness under it. Parallel to the existing valgrind
job, not a replacement: ASan catches stack/global overflows
valgrind misses, UBSan catches signed-overflow / alignment /
shift bugs neither valgrind nor `-Wall -Wextra` can see. CI
minutes are free; we run both.

PHP-specific gotchas to handle in the job:

- **`USE_ZEND_ALLOC=0`** must be exported before `make test` —
  PHP's pool allocator otherwise hides nearly every heap bug
  from ASan. Same env var the valgrind job already needs (or
  should — verify when wiring up).
- **`ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:halt_on_error=1`**
  so a single failure aborts the test run with a stack trace
  instead of being swallowed by `run-tests.php`.
- **`UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1`** — same
  reasoning. Default UBSan keeps running after a hit.
- LeakSanitizer (bundled with ASan) will likely flag PHP startup
  allocations as leaks. Suppress via an `lsan.supp` file
  committed to the repo (or `LSAN_OPTIONS=suppressions=...`)
  rather than disabling leak detection wholesale.
- Use `clang` for the sanitizer job — gcc's ASan is fine but
  clang's diagnostics and suppression file format are the
  reference; matches what most C projects standardise on.

### Debug PHP + `ZEND_RC_DEBUG` — CI job

Add a CI job that builds the extension against a **debug PHP**
(one configured with `--enable-debug`) and runs the phpt suite +
doc-snippet harness with:

```
ZEND_RC_DEBUG=1
ZEND_ALLOC_DEBUG=1   # extra alloc checks while we're at it
USE_ZEND_ALLOC=0     # same as the sanitizer job
```

What this catches that the other jobs don't:

- **Refcount leaks that aren't memory leaks** — forgotten
  `zval_ptr_dtor` on a return path. ASan/valgrind say "freed
  at shutdown, fine." Debug PHP says "leaked refcount."
- **Refcount underflows that don't yet crash** — extra
  `Z_DELREF` on a zval that still has other holders. Latent
  bug, silent under ASan today.
- **Persistent vs request-scoped confusion** — refcounting
  a `pemalloc(...,1)` value as if it were per-request.
- The thousands of `ZEND_ASSERT` macros throughout the engine
  that fire on protocol violations.

Surface area in NumPHP that benefits: every `PHP_METHOD` that
constructs and returns a new NDArray (comparisons, `where`,
reductions, shape ops, slice/concatenate), the NDArray class
object handlers, `fromArray` / `toArray` zval traversal, and
error paths that `RETURN_THROWS()` after partial setup. That's
most of the extension.

CI plumbing notes:

- Use the official `shivammathur/setup-php` action with
  `coverage: none` and the debug-symbols variant, or install
  `php-dev` against a debug build, or build PHP from source
  in the job (the heaviest option but most reproducible).
  Pick during sprint shaping based on what setup-php exposes.
- Debug PHP runs 3–5× slower than release. Acceptable for a
  CI job; budget ~10–15 min wall-time.
- Some `--EXPECTF--` blocks may need tweaking — debug builds
  emit slightly different stderr (memory-leak summaries at
  shutdown, etc.). Fix as they surface.
- Local dev unaffected. We do not require a debug PHP on the
  dev machine; CI is the gate.

### Valgrind — on-demand local, weekly cadence

User wants valgrind available locally but not on every build.
Plan:

- Install valgrind locally via apt (one-time; user will run
  the install — do not auto-install).
- Add a `make memcheck` target (or `scripts/memcheck.sh`)
  that wraps `run-tests.php -m` (run-tests has native valgrind
  integration) so `make memcheck` runs the full phpt suite
  under valgrind with `USE_ZEND_ALLOC=0`. Keeps the recipe out
  of muscle memory and out of `RESUME.md`.
- Document a recommended cadence in `docs/RESUME.md`: run
  `make memcheck` at sprint boundaries (e.g. before
  `/agile:wrap-sprint`) or before any release-candidate cut.
  Not enforced — opt-in.
- CI valgrind job stays exactly as it is today.

### Local sanitizer recipe

Document a one-liner in `docs/RESUME.md` (or a new
`docs/CONTRIBUTING.md` if one is warranted) for a local
sanitizer build, so devs can opt in during active work on a
suspect area:

```bash
make clean
phpize && ./configure --enable-numphp \
  CFLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g" \
  LDFLAGS="-fsanitize=address,undefined"
make
USE_ZEND_ALLOC=0 \
  ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 \
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  NO_INTERACTION=true make test
```

A `make sanitize` target wrapping the above would be nicer
than a docs blob — decide during sprint shaping.

## Acceptance Criteria

- `make CFLAGS="-Wall -Wextra -Werror -Wshadow -Wstrict-prototypes
  -Wmissing-prototypes -Wcast-align -Wnull-dereference
  -Wdouble-promotion -O2 -g"` builds clean (no warnings, no
  errors).
- All 67 phpt tests still pass; FFI skip still skipped; doc-snippet
  harness still green.
- CI workflow uses the same flag set; a deliberately-shadowed
  variable in a throwaway branch fails CI (sanity check, then
  reverted).
- New CI job runs the phpt suite + doc-snippet harness under
  `-fsanitize=address,undefined`, with `USE_ZEND_ALLOC=0` and
  appropriate ASAN/UBSAN_OPTIONS. A deliberately-introduced
  use-after-free / signed-overflow on a throwaway branch fails
  CI (sanity check, then reverted).
- New CI job runs the phpt suite + doc-snippet harness against
  a debug-built PHP with `ZEND_RC_DEBUG=1`, `ZEND_ALLOC_DEBUG=1`,
  `USE_ZEND_ALLOC=0`. A deliberately-introduced refcount leak
  (`Z_ADDREF` without matching `zval_ptr_dtor`) on a throwaway
  branch fails CI (sanity check, then reverted). Any phpt
  `--EXPECTF--` adjustments needed for debug-build stderr
  differences are listed in the sprint plan.
- `lsan.supp` (or equivalent) committed if PHP startup
  allocations need suppressing; each entry has a comment
  explaining why.
- `make memcheck` (or `scripts/memcheck.sh`) runs the phpt
  suite under valgrind locally. Documented in `RESUME.md` with
  a recommended cadence (e.g. "run at sprint boundaries").
- `make sanitize` (or a docs recipe) for a local ASan+UBSan
  build, mirroring the CI job.
- `docs/system.md` records the flag set + sanitizer policy as
  a decision; one-line note in `docs/RESUME.md` updates the
  "Build green at" line and points at the new local targets.
- Any per-file `#pragma GCC diagnostic` suppressions are listed
  in the sprint plan with a reason (so we know what we accepted).

## Sprint split

Sized into three small sprints — see `docs/sprints/`:

1. **19a — conservative warning flags.** `-Werror`, `-Wshadow`,
   `-Wstrict-prototypes`, `-Wmissing-prototypes`. Fix what
   surfaces.
2. **19b — ASan + UBSan + local convenience.** New CI job,
   `lsan.supp`, `make sanitize`, `make memcheck`.
3. **19c — debug PHP + `ZEND_RC_DEBUG`.** New CI job,
   `--EXPECTF--` tweaks for debug-build stderr.

**Deferred to a follow-up sprint** (lower-confidence flags
that may require source refactoring):

- `-Wcast-align` — likely fires in `nditer.c` / `ops.c` buffer
  pointer casts; the right fix (alignment assertion vs memcpy
  vs aligned attribute) is a project-wide pattern decision
  worth its own sprint.
- `-Wnull-dereference` — flow-sensitive, may demand spot null
  checks across multiple files.
- `-Wdouble-promotion` — likely fires in f32 paths; mostly
  literal suffix fixes (`0.0` → `0.0f`) but tedious.

These three together would form a future "warnings round 2"
sprint if/when the value justifies the work.
