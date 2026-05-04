---
sprint: 19c-debug-php
story: 19-build-quality-hardening (3 of 3)
date: 2026-05-04
version_target: 0.0.23
depends_on: 19b-asan-ubsan, 19b-fix-do-operation-leak
---

> **Note (refreshed 2026-05-04):** plan was originally drafted 2026-05-03
> targeting 0.0.21. Since then `19b-fix-do-operation-leak` shipped at
> 0.0.21 (closed the compound-assign leak, flipped LSan on, amended
> decision 37 in place) and `clean-rule-no-recursive-rm` shipped at
> 0.0.22. Current head is **0.0.22**, so this sprint targets **0.0.23**.
> Decision number for the new debug-PHP policy is still **38** — 19b-fix
> amended 37 rather than allocating a new number.

## Goal

Add a CI job that runs the test suite against a debug-built
PHP with `ZEND_RC_DEBUG=1` + `ZEND_ALLOC_DEBUG=1`. Catches
refcount-protocol bugs that ASan, UBSan, and valgrind can't
see (refcount leaks that aren't memory leaks; refcount
underflows that don't yet crash; persistent vs request-scoped
confusion).

By end of sprint:

- New CI job runs phpt + doc-snippets + examples against
  debug PHP.
- Any phpt `--EXPECTF--` blocks adjusted for debug-build
  stderr divergence.
- Any refcount or assertion bugs surfaced are fixed.
- One sanity-check branch (refcount leak) fails the new CI
  job and is reverted.

## In scope

### CI: debug-PHP job

1. Add `debug-php` job to `.github/workflows/ci.yml`.
2. Setup approach (decide during execution):
   - **Option A (preferred):** `shivammathur/setup-php@v2`
     debug variant if it exposes one cleanly.
   - **Option B (fallback):** build debug PHP from source in
     the job. Cache via `actions/cache` to amortise.
3. Job env on test step:
   ```
   ZEND_RC_DEBUG=1
   ZEND_ALLOC_DEBUG=1
   USE_ZEND_ALLOC=0
   NO_INTERACTION=true
   ```
4. Steps: build extension against debug PHP, `make test`, run
   examples and diff against `.expected`.

### Triage findings

5. **`--EXPECTF--` divergence** — debug PHP emits extra stderr
   (memory-leak summaries, allocation tracking). Widen
   patterns to absorb the new lines; one-line comment in the
   phpt explaining why.
6. **Refcount aborts** — `ZEND_RC_DEBUG` aborts with a trace.
   Fix in `src/`. Most refcount bugs aren't reproducible from
   PHP-land without debug PHP, so a phpt regression test is
   often impossible — document the fix in the sprint outcome
   instead.
7. **Persistent vs request-scoped confusion** — `pemalloc(...,1)`
   values being refcounted as if request-scoped. Fix at the
   alloc site.
8. **Other engine assertions** — fix or document.

### Sanity check

9. Throwaway branch: introduce a `Z_ADDREF` without matching
   `zval_ptr_dtor` in `src/ndarray.c`. Push. Confirm
   `debug-php` CI job fails. Delete branch.

### Docs

10. `docs/system.md` **decision 38**: debug-PHP CI policy —
    runs on every build; surfaces refcount-protocol violations
    nothing else catches; local dev does not need debug PHP.
11. `docs/RESUME.md` quality-cadence paragraph extended to
    mention the debug-PHP job runs on every CI build.
12. Story 19 file moved from `backlog/` → `done/` (since 19c
    closes the story).

### Version

13. Bump `src/numphp.h` from `0.0.22` to `0.0.23`.

## Out of scope

- Local debug-PHP build target — adds toolchain we don't need
  locally; CI is the gate.
- New compiler flags (sprints 19a / future).
- ASan / UBSan changes (sprint 19b shipped them).

## Implementation plan

### Setup

1. Try Option A (setup-php debug variant). If it works, use it.
   Otherwise fall back to Option B (build from source + cache).

### CI wiring

2. Add the `debug-php` job. Push.

### Triage

3. Watch the first run. Categorise failures:
   - EXPECTF mismatches → widen + comment
   - Refcount aborts → fix in src/
   - Other assertions → fix in src/
4. Iterate. Per memory: cap at 2 fix attempts per finding;
   defer if unbounded. Document anything deferred.

### Sanity check

5. Refcount-leak branch. Confirm red. Delete.

### Wrap

6. Bump version. Update `RESUME.md`. Write decision 38.
7. Move `19-build-quality-hardening.md` to `done/`.

## Acceptance criteria

- `debug-php` CI job runs phpt + doc-snippets + examples
  against debug PHP with `ZEND_RC_DEBUG=1` +
  `ZEND_ALLOC_DEBUG=1` + `USE_ZEND_ALLOC=0`; green on `main`.
- Any phpt `--EXPECTF--` adjustments inline-commented.
- Any refcount/assertion bugs found are fixed in `src/` or
  explicitly documented as deferred (with reason).
- Refcount-leak sanity-check branch fails CI; deleted, noted
  in sprint outcome.
- `docs/system.md` decision 38 written.
- `docs/RESUME.md` quality cadence updated.
- `src/numphp.h` → `0.0.23`.
- Story 19 moved to `done/`.

## Risk register

- **setup-php debug variant unavailable** → Option B
  (build from source + cache). Adds ~5 min cold-cache.
- **Debug PHP surfaces a non-trivial refcount bug** — fix if
  scoped; defer to a hot-fix sprint if multi-day.
- **EXPECTF widening becomes excessive** — if more than ~5
  tests need adjustment, document the pattern as a decision
  sub-bullet (e.g. "all reduction tests widened for debug
  shutdown stderr") instead of adjusting per-file.
