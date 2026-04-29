# System Documentation

Maintained by `/agile:wrap-sprint`. Cross-cutting decisions, sprint outcomes, and the long-lived "keeper" record for numphp. Read this to understand the system without reading all the code.

## Architectural Decisions

### 2026-04-28 — Sprint: Project Scaffolding & Architecture Lock-in

#### 1. PHP Target & Thread Safety
- **Decision:** PHP 8.2+, NTS only for v1. ZTS is deferred until a thread-safety review of the C state has been completed.
- **Enforcement:** Belt and braces — `config.m4` errors at `./configure` time when invoked against a ZTS PHP, and `numphp.h` raises `#error` if compiled in a ZTS context.

#### 2. dtype Scope (v1)
- `float32`, `float64`, `int32`, `int64`.
- Deferred: bool, complex64 / complex128, float16.

#### 3. dtype Promotion (binary ops)

| op1 \ op2 | f32 | f64 | i32 | i64 |
|-----------|-----|-----|-----|-----|
| **f32**   | f32 | f64 | f32 | f64 |
| **f64**   | f64 | f64 | f64 | f64 |
| **i32**   | f32 | f64 | i32 | i64 |
| **i64**   | f64 | f64 | i64 | i64 |

Rule: `int + int → wider int`; `int + float → float of at least equal precision`; mixed int/float widths default to `f64`. Matches NumPy.

#### 4. Exception Hierarchy

```
\NDArrayException          (base, extends \RuntimeException)
├── \ShapeException        shape / dimension mismatch
├── \DTypeException        unsupported or incompatible dtype
└── \IndexException        out of bounds, invalid slice
```

Registered in `MINIT`. `numphp.c` is the canonical declaration site; subclass `class_entry` pointers are exposed as externs in `numphp.h`.

#### 5. Memory Ownership Model
- Refcounted underlying data buffer (defined in Story 2), decoupled from the `numphp_ndarray` metadata shell.
- Views never free the buffer; they hold a refcount on it. Owners decrement on free.
- No copy-on-write in v1 — explicit `copy()` is the user's escape hatch.

#### 6. Contiguity Tracking
- `flags` bitfield on `numphp_ndarray`: `C_CONTIGUOUS`, `F_CONTIGUOUS`, `WRITEABLE`.
- Recomputed in any op that produces a view; preserved across no-copy ops.

#### 7. Error Policy
- Float division by zero → IEEE 754 (`inf`, `nan`); no exception.
- Int division by zero → `\DivisionByZeroError`.
- Out-of-bounds index → `\IndexException`.
- Ragged `fromArray` input → `\ShapeException`.

### 2026-04-29 — Sprint: Statistical & Mathematical Functions

#### 8. round-half semantics (deliberate divergence from NumPy)

`NDArray::round()` rounds **half-away-from-zero** (PHP's `PHP_ROUND_HALF_UP`). NumPy uses banker's rounding (half-to-even).

| Value | NumPy | numphp / PHP |
|-------|-------|--------------|
| `0.5` | `0.0` | `1.0` |
| `1.5` | `2.0` | `2.0` |
| `2.5` | `2.0` | `3.0` |
| `-0.5` | `-0.0` | `-1.0` |

**Rationale:** PHP users expect `round(0.5) === 1.0` from the scalar `round()`. Forcing them to remember which `round()` they're calling produces silent off-by-one bugs. The cost of divergence is paid once at documentation time; the cost of matching NumPy would be paid on every test, port, and bug report. **Locked by `tests/036-elementwise-math.phpt`** — any future change to banker's rounding will fail loudly. See implementation in `ops.c::round_half_away_from_zero`.

#### 9. Output dtype rules for reductions

| Op | int input | f32 input | f64 input |
|----|-----------|-----------|-----------|
| `sum` | int64 | f32 | f64 |
| `mean` / `var` / `std` | f64 | f32 | f64 |
| `min` / `max` | preserve | preserve | preserve |
| `argmin` / `argmax` | int64 | int64 | int64 |

`sum` of int promotes to int64 to avoid silent overflow on int32 accumulation. Argmin/argmax always return int64 regardless of input dtype.

#### 10. NaN policy in reductions

Default reductions propagate NaN: any NaN in input → NaN in output. NaN-aware variants (`nansum`, `nanmean`, …) skip NaN inputs.

| All-NaN slice | Behavior |
|---|---|
| `nansum` | `0` (NaNs treated as additive identity) |
| `nanmean` / `nanmin` / `nanmax` | `NaN` (no data) |
| `nanargmin` / `nanargmax` | throw `\NDArrayException` |

Integer dtypes have no NaN; nan-variants on int dtypes are aliases of the regular forms (wired at the PHP_METHOD wrapper).

#### 11. Sort stability

`sort` and `argsort` use `qsort` and are **not stable** for equal keys. The order of equal elements is unspecified. Tests use distinct values to avoid relying on tie-break behavior. If a user need arises for stable sort, switch the implementation to a mergesort variant.

## Open Items / Caveats

## Open Items / Caveats

- **LAPACK symbol portability on macOS.** `config.m4` probes for `dgetri_` (Fortran name-mangling, the Linux convention). On macOS Accelerate / some OpenBLAS builds the symbol is `dgetri` (no underscore). The macOS CI lane runs `continue-on-error: true`. To make macOS blocking, the probe needs a fallback that tries both names.
- **gcov coverage gate parked.** CI runs the gcov job as `continue-on-error: true` until Story 2 lands substantive code. Flip to blocking when `ndarray.c` has real implementation.
- **Nothing depends on `nditer.c` yet.** The empty TU is listed in `PHP_NEW_EXTENSION` so the build graph stays stable. If a header declaration is added without a definition the linker fails loud — that is the intended canary.

## Sprint Outcomes

- **2026-04-28 — `project-scaffolding-architecture`** delivered the buildable extension skeleton, four registered exception classes, a CI matrix (build + valgrind + gcov + macOS non-blocking), and the seven decisions above.
- **2026-04-28 — `ndarray-struct-and-creation-api`** delivered the refcounted buffer + ndarray struct, `NDArray` class with create/free/clone handlers (clone deep-copies through strides), the full creation API (`zeros`, `ones`, `full`, `eye`, `arange`, `fromArray`), metadata accessors (`shape`/`dtype`/`size`/`ndim`), and `toArray` (pulled forward from Story 11 Phase A so contents could be asserted in phpt). 8 new phpt tests, all green; build clean at -Wall -Wextra. `NUMPHP_MAX_NDIM = 16` adopted as a static cap to avoid heap-allocating dimension arrays per-op. ArrayAccess and Countable interfaces remain deferred to Story 4 because they require offsetGet/offsetSet handlers tied to indexing semantics.
- **2026-04-28 — `indexing-and-slicing`** delivered `ArrayAccess` and `Countable` on `NDArray`, view allocator that shares the parent's refcounted buffer, `read_dimension` / `write_dimension` / `has_dimension` / `unset_dimension` / `count_elements` C handlers, the matching PHP method stubs (`offsetGet` etc.), and `NDArray::slice($start, $stop, $step = 1)` for axis-0 views with negative-index normalisation. 5 new phpt tests; `$a[$i] = $value` supports scalar (1D), scalar broadcast (nD), and shape-matched `NDArray` RHS. Negative `slice` step, multi-axis tuple slicing, and PHP-array RHS for assignment are deferred to later sprints. `offsetGet` arginfo declares `IS_MIXED` return to satisfy the `ArrayAccess::offsetGet(mixed): mixed` interface contract — without this PHP 8 emits a deprecation that breaks every phpt because phpt diff is byte-exact.
- **2026-04-28 — `broadcasting-and-elementwise-ops`** combined Stories 7 + 5 to ship the broadcast-aware nd-iterator and the four arithmetic ops at once. `numphp_nditer` walks up to 4 operands with virtual zero strides for broadcast dims; `numphp_broadcast_shape` and `numphp_promote_dtype` are now load-bearing. Operator overloading via `do_operation` covers `+ - * /` for `NDArray ⊕ NDArray`, `NDArray ⊕ scalar`, and `scalar ⊕ NDArray` (scalars are wrapped in 0-D temp arrays so the iterator handles them uniformly). Float divzero follows IEEE; int divzero throws `\DivisionByZeroError`. 6 new phpt tests, all green; build clean at -Wall -Wextra. True buffer-mutating in-place methods (`addInplace` etc.) deferred — `$a += $b` works via PHP's default compound-assignment path (creates a new array).
- **2026-04-28 — `shape-manipulation`** delivered `reshape` (with `-1` inference, contiguity-aware view-vs-copy), `transpose` (default reverse + explicit permutation; always a view), `flatten` (contiguous 1-D copy), `squeeze` / `expandDims` (size-1 dim manipulation; views), and the static `concatenate` / `stack` (copy-based, dtype-promoted across inputs). Helper `materialize_contiguous` is shared between reshape's non-contig path and any future op that needs a contiguous copy. `NUMPHP_CONCAT_MAX = 64` caps the number of inputs to `concatenate` / `stack`, stack-allocated for cache friendliness. 6 new phpt tests; all 27 tests green.
- **2026-04-28 — `blas-integration`** wired `dot`, `matmul`, `inner`, `outer` to OpenBLAS. Pure float32 inputs use the s-path (`cblas_sdot` / `cblas_sgemm` / `cblas_sger`); everything else promotes to float64 and uses the d-path. The promotion rule is "if either input is non-f32 (i.e. f64 or any int), promote both to f64" — wider than the elementwise table because BLAS has no integer routines. `ensure_contig_dtype` materialises non-contiguous or wrong-dtype inputs to fresh contiguous owners before the BLAS call. cblas.h lives at `/usr/include/x86_64-linux-gnu/cblas.h` on Ubuntu (multi-arch); GCC finds it without an explicit `-I`. Deferred: 3D+ batched matmul, native-int matmul, and strided BLAS (passing `lda` instead of copying for transposed views with unit inner stride). 4 new phpt tests; all 31 tests green.
- **2026-04-29 — `stats-and-math-functions`** delivered the full reduction surface (`sum`/`mean`/`min`/`max`/`var`/`std`/`argmin`/`argmax`) with `axis` + `keepdims`, NaN-aware variants of all eight, element-wise math (`sqrt`/`exp`/`log`/`log2`/`log10`/`abs`/`power`/`clip`/`floor`/`ceil`/`round`), and `sort`/`argsort`. `var` and `std` use Welford's online algorithm; `mean` uses pairwise sum (recursive halving with leaf 8). `ops.c` is no longer a stub — all kernels live there; `ndarray.c` keeps thin PHP_METHOD wrappers and the method table. `read_scalar_at`, `write_scalar_at`, and `materialize_contiguous` were promoted to exported `numphp_*` symbols so `ops.c` can reuse them. Sort is `qsort`-backed and unstable for equal keys (documented in decision 11). Decision 8 records the deliberate divergence from NumPy on round-half semantics — locked by a dedicated test. The `axis_zv == NULL` C-pointer trick distinguishes "argument omitted" from "explicit `null`" in `sort`/`argsort` (omitted → axis = -1, explicit null → flatten). 7 new phpt tests; all 38 tests green; build clean at -Wall -Wextra. Deferred: array-valued power exponent, multi-axis tuple reductions, median/percentile/quantile, stable sort, in-place sort, cumsum/cumprod.
