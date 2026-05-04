---
story: Bool surface follow-up — any / all / bitwise / logical / prod / count_nonzero / ptp
created: 2026-05-04
---

## Description

Story 17 shipped the bool dtype, six comparison ops, and `where`.
What's still missing to make bool *useful* is the cluster of
operations that produce, consume, and reduce bool arrays:

- **`any` / `all`** — the natural reduction over a mask.
- **Bitwise ops** (`&` / `|` / `^` / `~`) — natural on bool and on
  int. NumPy spells these `bitwise_and` / `bitwise_or` / `bitwise_xor`
  / `bitwise_not`.
- **Logical ops** (`logical_and` / `logical_or` / `logical_xor` /
  `logical_not`) — like bitwise but lift any non-zero numeric input
  to bool first; output is always bool.
- **`prod`** — the multiplicative twin of `sum`. Closes a small but
  visible gap in the reduction surface.
- **`count_nonzero`** — utility that consumes masks (and arbitrary
  numeric input).
- **`ptp`** — peak-to-peak (`max - min`). Tiny but commonly used.

This is the "easy bucket" cluster from `docs/numpy-parity-roadmap-2026-05-01.md`
— pure expansion of existing reduction + element-wise patterns, all
unblocked by the bool dtype that landed in Story 17. No
architectural lifts. One sprint covers all eleven new methods.

The point is not the individual ops — each is small. The point is
that **after this sprint, masks are first-class**: you can build
them with comparisons (Story 17), combine them with bitwise/logical,
reduce them with `any`/`all`/`count_nonzero`, and feed them to
`where`. The only piece still missing is consuming a mask as an
*index* — that's Story 21 (boolean mask indexing).

### What's in scope

#### Reductions

- **`NDArray::any(?int $axis = null, bool $keepdims = false): bool|NDArray`** —
  short-circuiting OR over the array. Returns a bool scalar when
  `$axis === null`, a bool NDArray otherwise.
- **`NDArray::all(?int $axis = null, bool $keepdims = false): bool|NDArray`** —
  short-circuiting AND.
- **`NDArray::prod(?int $axis = null, bool $keepdims = false): mixed`** —
  multiplicative reduction. Output dtype follows the same rule
  decision 31 locked for `cumprod`: int → `int64` (silent-overflow
  protection, deliberate divergence from NumPy). f32 → f32, f64 → f64,
  bool → int64 (consistent with `sum`).
- **`NDArray::nanprod(?int $axis = null, bool $keepdims = false): mixed`** —
  NaN-skipping variant. Treats NaN as the multiplicative identity (1).
  Symmetric with `nansum` (which treats NaN as additive identity 0)
  and `nancumprod`. Integer dtypes alias `prod` (no NaN possible).
- **`NDArray::countNonzero(?int $axis = null, bool $keepdims = false): int|NDArray`** —
  counts non-zero (or non-False) elements. Output dtype: `int64`
  always. NaN counts as non-zero (matches NumPy: `bool(NAN) === true`).
- **`NDArray::ptp(?int $axis = null, bool $keepdims = false): mixed`** —
  peak-to-peak: `max - min`. Output dtype = input dtype (no
  promotion). Bool input → int8 (`true - false = 1`)? Match NumPy:
  `np.ptp(np.array([True, False]))` returns `True` (bool, since
  `True - False` in bool is True). Decide during shaping.

For all six: `axis` must be in range `[-ndim, ndim)`; out of range
throws `\ShapeException`. Empty input: `any` → false, `all` → true,
`prod` → 1 (cast to output dtype), `countNonzero` → 0, `ptp` →
throws `\ShapeException` (no defined value; matches NumPy raising
`ValueError`).

#### Bitwise ops (static methods, NDArray-or-scalar inputs)

- **`NDArray::bitwiseAnd(NDArray|int|bool $a, NDArray|int|bool $b): NDArray`** —
  element-wise `&`. Inputs must be int or bool dtype. f32/f64 input
  throws `\DTypeException` (matches NumPy: `bitwise_and` on float
  raises `TypeError`).
- **`NDArray::bitwiseOr`**, **`NDArray::bitwiseXor`** — same shape.
- **`NDArray::bitwiseNot(NDArray|int|bool $a): NDArray`** — unary
  `~`. Same dtype constraint.

Broadcasting + dtype promotion follow the existing 5×5 table (no new
entries needed; bitwise rejects float so the relevant slice is just
{bool, int32, int64}).

**Bool ⊕ bool semantics:** stays bool. (`bool & bool = bool`, etc. —
matches NumPy and the Story-17 write-canonicalisation idiom.)

#### Logical ops (static methods, any numeric/bool input → bool output)

- **`NDArray::logicalAnd(NDArray|int|float|bool $a, NDArray|int|float|bool $b): NDArray`**
- **`NDArray::logicalOr`**, **`NDArray::logicalXor`**
- **`NDArray::logicalNot(NDArray|int|float|bool $a): NDArray`**

Inputs are coerced to bool element-wise: any non-zero numeric →
true, zero → false, NaN → true (matches NumPy / PHP `(bool)NAN`).
Output dtype is always bool. Broadcasting follows the existing
rules.

### Operator overloading

**Out of scope.** Per decision 35 (Story 17), comparison ops are
method-only — `==` is reserved for object identity per Zend handler
contract. Same reasoning applies to `&` / `|` / `^` / `~`: PHP's
`do_operation` could in principle dispatch them, but expanding the
`do_operation` surface right after closing the compound-assign leak
(sprint 19b-fix) is unattractive risk/reward. Method-only is the
right v1 stance; `do_operation` for bitwise can be a follow-up if
demand surfaces.

### What's out of scope (deliberately)

- **Boolean mask indexing** — the consumer side of masks. That's
  Story 21 (the architectural lift; generalises `offsetGet` /
  `offsetSet`). This story produces and reduces masks; consuming
  them as indices is the next sprint.
- **Shift ops** (`<<`, `>>` / `bitwise_left_shift` / `bitwise_right_shift`).
  Trivial to add but deferred until anyone asks; saves arginfo +
  test churn this sprint.
- **`isclose` / `allclose` / `array_equal` / `array_equiv`** — also
  bool-producing, but they're tolerance comparisons (not bitwise/
  logical). Worth a sprint of their own paired with `isnan` / `isinf` /
  `isfinite`.
- **`logical_xor` short-circuit** — XOR can't short-circuit by
  definition, so it walks the full inputs. Documented, not deferred.
- **In-place variants** — consistent with the broader v1 stance
  (decision held since Story 5).
- **`do_operation` overloads** for `&` / `|` / `^` / `~` — see
  rationale above.

### Decisions to lock during the sprint

- **Decision 39** — `prod` on int promotes to `int64` (extends the
  decision-31 rationale for `cumprod`; same divergence from NumPy,
  same silent-overflow concern).
- **Decision 40** — bitwise ops reject float input (matches NumPy;
  alternative would be implicit cast to int which is too surprising).
- **Decision 41** — logical ops always output bool regardless of
  input dtype (matches NumPy; consistent with comparisons).

If any of these turn out to be one-line consequences of decisions
17/31/35 rather than new policy, fold them in instead of allocating
new numbers — exercise judgement during execution.

## Acceptance Criteria

- All eleven new public methods exist on `NDArray` with the
  signatures listed above; arginfo declared correctly; all are
  recognised by reflection.
- **Reductions:**
  - `any` / `all` on bool 1-D, 2-D, 3-D inputs with axis = `null` /
    integer / negative; empty-input cases return correct identity.
  - `prod` / `nanprod` cover all four numeric dtypes × axis cases ×
    NaN propagation × all-NaN slice (returns 1, no exception);
    int → int64 promotion verified by test asserting `dtype()`.
  - `countNonzero` cover bool / int / float / NaN edge cases; output
    dtype is `int64` always.
  - `ptp` covers all dtypes; empty-input throws `\ShapeException`
    with a clear message.
- **Bitwise:**
  - `bitwiseAnd` / `Or` / `Xor` / `Not` cover bool ⊕ bool, int ⊕ int,
    bool ⊕ int (promotes), broadcasting, scalar-on-either-side.
  - f32 / f64 input throws `\DTypeException` with a message naming
    the offending dtype.
- **Logical:**
  - `logicalAnd` / `Or` / `Xor` / `Not` cover bool, int, float (incl.
    NaN), broadcasting, scalar-on-either-side; output is always bool.
- **Phpt coverage:** at least 4 new phpt files (one per cluster:
  `064-any-all.phpt`, `065-prod-nanprod.phpt`,
  `066-bitwise.phpt`, `067-logical.phpt`); existing 67 tests still
  pass; doc-snippet harness still green.
- **Build:** clean at the canonical CFLAGS
  (`-Wall -Wextra -Werror -Wshadow -Wstrict-prototypes
  -Wmissing-prototypes -O2 -g`).
- **Sanitizers:** new code passes `scripts/sanitize.sh` (ASan +
  UBSan + LSan with `detect_leaks=1`); CI sanitizer + debug-PHP
  jobs green.
- **Docs:**
  - `docs/api/ndarray.md` — new sections for the eleven methods,
    each with signature / params / returns / throws / runnable
    example.
  - `docs/concepts/dtypes.md` — reductions table extended (`prod`,
    `nanprod`, `countNonzero`, `ptp`); bitwise dtype-rejection
    rule documented.
  - `docs/cheatsheet-numpy.md` — eleven new rows; `prod` divergence
    flagged like `cumprod`.
- **Decisions** — 39 / 40 / 41 written into `docs/system.md` (or
  folded into existing decisions if they collapse).
- **Version** bumped to **0.0.24** (or whichever number the
  shaping picks; one bump per sprint per the established cadence).

## Sprint split

**Two sprints, decided at shaping (2026-05-04):** the user opted to
split this story rather than bundle, citing the project's success
with efficient ceremony at small-sprint cadence. The two halves
touch different code regions and have independent decision arcs, so
the split is honest, not arbitrary.

### Sprint 20a — reductions

`any`, `all`, `prod`, `nanprod`, `countNonzero`, `ptp` — 6 methods
reusing `numphp_reduce` / `do_reduce_method` in `ops.c` /
`ndarray.c`. Decision 39 (`prod` int → int64). ~450 lines new.
Plan: `docs/sprints/20a-bool-reductions.md`. Target 0.0.24.

### Sprint 20b — bitwise + logical (to be shaped after 20a wraps)

`bitwiseAnd` / `Or` / `Xor` / `Not` (4 methods, reject float input)
plus `logicalAnd` / `Or` / `Xor` / `Not` (4 methods, coerce-to-bool
input, bool output). Reuse the elementwise broadcasting iterator.
Decisions 40 (bitwise rejects float) and 41 (logical always outputs
bool). ~320 lines new. Plan to be shaped after 20a wraps. Target
0.0.25.
