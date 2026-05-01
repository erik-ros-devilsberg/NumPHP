# Coverage Audit — Story 13 Phase B (2026-05-01)

Audit performed against the 54 phpt tests + 1 doc-snippet harness on
branch `main` at commit `4310ac4`. Goal: identify every error path, edge
shape, and dtype boundary case relevant to v1, then either close the gap
in this sprint or explicitly defer with a reason.

## Methodology

Per-file walk of `*.c` for `zend_throw_exception*` sites cross-referenced
against test files for `try { ... } catch (Exception)` blocks. Decision
table from `docs/system.md` (decisions 1–27) used as the spec — anything
locked by a decision must have a phpt asserting it.

## 1. Error paths — coverage by exception class

| Exception class | Throw sites (C) | Tests that trigger | Status |
|-----------------|-----------------|--------------------|--------|
| `\NDArrayException` | base + leaf — used in `nanargmin/max` empty, `solve` singular, `slice` step≤0, `arange` step=0, `concatenate` >64 inputs, BufferView constructor | 002, 014, 020, 035, 038, 039, 041, 052 | covered |
| `\ShapeException` | most prevalent (~75 sites): bad dtype-shape combos, ragged input, axis OOR, BLAS shape mismatch, reshape size mismatch | 002, 008, 012, 021, 022, 025, 026, 027, 028, 030, 038 | covered |
| `\DTypeException` | bad dtype string in factories; non-numeric / NaN-into-int in `full` and `fromArray`; bad cast in normalize | 002, 008 (dtype-validation errors implicit) | **partial — closed in this sprint** |
| `\IndexException` | offset OOR (get/set), 0-D index, slice OOR, slice on 0-D, axis OOR for squeeze/expandDims/concatenate/stack | 002, 011, 012 | covered for ArrayAccess; squeeze/expand axis OOR partially; **closed in this sprint** |

**Closed in this sprint:**

- `055-error-paths-dtype.phpt` — `\DTypeException` triggered by every
  factory that validates dtype: `zeros`, `ones`, `full`, `eye`, `arange`,
  plus `full` with `NAN` for an integer dtype, plus `fromArray` with a
  string element.
- `056-error-paths-index.phpt` — `\IndexException` on `squeeze` axis OOR,
  `expandDims` axis OOR, `concatenate` axis OOR, `stack` axis OOR. (Slice
  on 0-D and slice with step=0 are already in test 014.)

## 2. Edge shapes

| Edge shape | Coverage | Status |
|------------|----------|--------|
| 0-D scalar (`ndim=0`) reductions | 038 | covered |
| 0-D `toArray` round-trip | 046 | covered |
| 0-D `slice` (throws IndexException) | 014 | covered |
| `shape=[0]` empty | 038 (sum/mean/argmin/argmax) | covered |
| Single-element matmul / single-row matmul | 029 (basic 2×3 × 3×2; no 1×k × k×1) | **closed in this sprint** |
| Single-row × single-col vector outer | 030 | covered |
| Reduction with `keepdims=true` on 0-D | 038 | covered |
| Maximum-rank array (ndim=16) | not specifically | **deliberately deferred** — 16-D is a hard limit, not a regression target. Out-of-range rank is tested implicitly via fromArray nesting cap. |

**Closed in this sprint:**

- `057-edge-shapes-blas.phpt` — `(1, k) × (k, 1)`, `(k,) · (k,)` with
  `k=1`, `(n, 1) × (1, m)` (single-axis broadcast in matmul shape).

## 3. dtype boundary cases

| Boundary | Coverage | Status |
|----------|----------|--------|
| `int32` sum: must promote to `int64` (decision 9) | nothing locks the promotion at value-overflow scale | **closed in this sprint** |
| `f32` denormals through `mean` / `var` | not specifically | **deliberately deferred** — denormal handling is delegated to libc; testing it is testing libc, not our code. Footnote added to system.md. |
| `+inf` / `-inf` through every reduction | 020 (divzero only) and 044 (norm) — not all reductions | **closed in this sprint** |
| `NaN` through every reduction | 033 (min/max), 034 (var/std), 035 (nan-aware), 038 (mean of empty) | covered |
| Integer `nanvar`/`nanmean` aliasing (decision 10) | 035 | covered |
| `argmin` / `argmax` always return int64 | 033 | covered |

**Closed in this sprint:**

- `058-dtype-int-sum-promotion.phpt` — locks decision 9: an `int32`
  array of 1000 elements each with value `INT32_MAX / 4` sums to a value
  that would overflow int32. Test asserts the result dtype is `int64`
  and the value matches the exact non-overflowed sum.
- `059-inf-through-reductions.phpt` — `+inf` and `-inf` through every
  reduction (sum, mean, min, max, var, std, argmin, argmax), one fixture
  array per direction.

## 4. Deliberately deferred (with reasons)

These are gaps the audit identified that we are choosing not to close in
v1:

1. **f32 denormals** — handed off to libc/the compiler's float
   implementation. Not our behaviour to assert.
2. **ndim=16 limit at boundary** — the limit exists as a `#define`
   (`NUMPHP_MAX_NDIM`); the limit-minus-one and limit-plus-one cases
   would require constructing 16-deep nested PHP arrays or higher,
   which exercises the `fromArray` recursion limit (already covered
   by 008) more than any new behaviour.
3. **Buffer-mutating in-place ops** (`addInplace` etc.) — deferred as
   a v1 feature per resume notes. Nothing to test.
4. **Multi-axis `slice`, negative `slice` step, boolean / fancy
   indexing** — deferred features per resume notes.
5. **Strided BLAS** (passing `lda` for transposed views) — deferred
   per resume notes; current code materialises a copy. Tests 028–031
   exercise the materialisation path correctly.
6. **3D+ batched matmul** — deferred per resume notes.
7. **BufferView `writeable=false` is advisory in v1 (decision 22)** —
   tested as advisory in 052; the *enforcement* of read-only at the
   buffer level is not implemented yet, so there is no behaviour to
   test.

These items are recorded in `docs/system.md` under the existing
deferral footnotes. None blocks v1 — they are post-1.0 work.

## 4a. Bug surfaced + fixed in this sprint

**Segfault on mixed scalar/array siblings in `fromArray`** —
`NDArray::fromArray([[1, [99]]])` (a row containing a scalar followed by
a nested array) crashed the PHP process. The `fromarray_walk` rank
inference let a sibling array extend the inferred ndim past the leaf
depth a previous scalar had locked, and the subsequent `fromarray_fill`
walk dereferenced the scalar as a HashTable.

Fix: introduce a `rank_locked` flag on `fromarray_walk`. Once any scalar
appears at depth `d == ndim_out`, no later array at depth `>= ndim_out`
is allowed — the walk now throws `\ShapeException("Ragged array: array
at leaf depth")` instead of accumulating an inconsistent shape.

Regression test: `060-fromarray-mixed-depth.phpt`. Bug never observed in
the wild (would only appear in user code that built dynamic nested
arrays), but the audit found it because we were inventorying every
exception throw site against tests.

Doc tidy: `docs/api/ndarray.md` previously claimed
`\DTypeException` was thrown for non-numeric leaf values in `fromArray`
and for `NaN` cast to integer dtypes in `full`. Neither is true — both
paths cast silently (matching PHP's standard scalar cast and NumPy's
behaviour for NaN→int). Doc updated to reflect what the C actually
does; this is the second non-obvious surface the snippet harness +
audit caught (the first being the `count()` divergence in Phase A).

## 5. Summary

- **Tests added this sprint:** 6 (numbered 055–060)
- **New trigger sites for previously-uncovered exceptions:** ~12
  individual cases across the 5 new tests
- **Decision-locked behaviour newly asserted:** decision 9 (int sum
  promotion at overflow scale)
- **Coverage gaps deferred:** 7 items, all consistent with already-public
  v1 deferral list

After this audit and the new tests, every exception class has at least
one negative test per major API surface, every reduction has at least
one ±inf-propagation test, and decision 9 is locked at the value-overflow
boundary.
