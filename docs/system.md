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

### 2026-04-29 — Sprint: Linear Algebra Module

#### 12. Raw LAPACK over LAPACKE

Linalg calls Fortran-mangled LAPACK symbols directly (`dgetrf_`, `dgesv_`, `dgesdd_`, `dgeev_`, `sgetrf_`, …). LAPACKE was rejected because (a) macOS Accelerate doesn't ship LAPACKE, and (b) it strips control over layout / error reporting. NumPy made the same choice; we follow.

#### 13. LAPACK symbol portability via `lapack_names.h`

Linux and reference LAPACK export Fortran-mangled symbols with a trailing underscore (`dgetri_`); macOS Accelerate exports without (`dgetri`). `config.m4` probes both names. If only the no-underscore form resolves, `AC_DEFINE([NUMPHP_LAPACK_NO_USCORE], [1])`. `lapack_names.h` then `#define`s every symbol Story 10 touches to the bare name, so call sites in `linalg.c` always write the underscored form. **Adding a new LAPACK routine requires a one-line addition to `lapack_names.h`** — single source of truth.

#### 14. Layout strategy — uniform F-contig copy

Every linalg op materialises 2-D inputs into an F-contiguous (column-major) scratch buffer of the right dtype, calls LAPACK, then transpose-copies the result back to a row-major `NDArray`. The "transpose trick" (interpret row-major bytes as column-major to skip the copy) works for `inv` and `det` but **fails for multi-RHS `solve`, `svd`, and `eig`** — see open item below for the math. Uniform col-major copy keeps the code simple and correct. Zero-copy fast path on `inv`/`det` and strided LAPACK (passing `LDA` for views) are deferred.

#### 15. eig — real eigenvalues only in v1

`Linalg::eig` throws `\NDArrayException` if any returned eigenvalue has nonzero imaginary part. Exception message names the offending index and the imaginary magnitude — informative enough for users to diagnose. Workaround until v2 lands complex dtype: ensure input is symmetric (`A == A^T`).

#### 16. SVD — thin only in v1

`Linalg::svd` always uses `JOBZ='S'`. Output shapes: `U` is `(m, k)`, `S` is `(k,)`, `Vt` is `(k, n)`, where `k = min(m, n)`. Full SVD (`JOBZ='A'`) deferred — adding a `full_matrices = true` keyword later is non-breaking.

#### 17. norm — Frobenius substitutes for matrix-2 in v1

`Linalg::norm($matrix, 2)` returns the Frobenius norm, not the spectral (largest singular value) norm. The spectral norm requires running a full SVD; defer until a user needs it. Vector-2 norm (Euclidean) is unaffected.

### 2026-04-29 — Sprint: Interoperability Phase A

#### 18. File I/O goes through PHP's stream layer

All file reads and writes use `php_stream_open_wrapper(path, mode, 0, NULL)` instead of plain `fopen`. This honours `open_basedir`, supports stream wrappers (`phar://`, `data://`, user-defined wrappers), and integrates with PHP's error reporting. Cost: ~10 extra lines of glue per call site. Benefit: eliminates the path-traversal / sandboxing footgun that would otherwise be a documented limitation. We pass flags `0` (not `REPORT_ERRORS`) so we own the error path via `\NDArrayException` — emitting both a PHP warning AND throwing an exception is duplicate noise.

#### 19. Locale-safe float formatting in CSV

Reader uses `zend_strtod` (locale-independent — always accepts `.` as decimal separator). Writer brackets its inner row loop with `setlocale(LC_NUMERIC, "C")` save/restore so `1.5` is always written as `1.5`, never `1,5`. Without these, a user with German locale (`LC_NUMERIC=de_DE`) would produce CSVs that even our own reader couldn't parse. Locked by a phpt that flips `LC_NUMERIC` and verifies bit-exact round-trip (best-effort — falls through cleanly if no European locale is installed on the test runner).

#### 20. Binary format — versioned, little-endian, extensible

The `NUMPHP\0\v` binary format has 16 bytes of header: 7-byte magic + 1-byte format version + 1-byte dtype + 1-byte ndim + 6 reserved bytes + int64 little-endian shape array. Loader checks the version byte FIRST, before any other parsing. Forward extensions land in the 6 reserved bytes; bumping the version byte is reserved for breaking changes only. Loader/saver share `#define NUMPHP_BINARY_FORMAT_VERSION 1` from `io.h` so they can't drift. Big-endian platforms get a compile-time `#error` to make the v1 deferral explicit.

#### 21. `php_fgetcsv` contract

`php_fgetcsv` does NOT read from the stream itself — it parses a buffer that the caller already filled via `php_stream_get_line`. The function `efree`s the buffer when stream is non-NULL (via the multi-line continuation path in `efree(buf); buf = new_buf`). **Caller MUST NOT efree the buf afterwards** — that is a double-free. Cell values are independent zend_strings (copied from temp scratch), so the returned HashTable is independent of buf. Released via `zend_array_release` (refcount-aware, the right pairing with `zend_new_array`).

### 2026-04-29 — Sprint: Interoperability Phase B (BufferView)

#### 22. `$writeable` is advisory in v1

`BufferView` exposes `$writeable` as a contract bool to the FFI consumer but does NOT enforce read-only on the source `NDArray`. Three reasons:
1. `NUMPHP_WRITEABLE` flag in `numphp_ndarray->flags` is already informational — no code path checks it.
2. Adding enforcement everywhere is scope creep relative to the goal of "expose a buffer to FFI."
3. FFI is already the trust boundary; documenting the contract is practically equivalent to enforcement.

The flip-side: Phase B does NOT track outstanding `writeable=false` views on a source array (no per-array view counter). If a future user need arises for hard enforcement, that's the place to start.

#### 23. BufferView captures metadata at construction; not auto-updated

`$ptr`, `$dtype`, `$shape`, `$strides`, `$writeable` are populated once when `bufferView()` is called and never refreshed. If the source `NDArray` is reshaped/transposed/mutated afterwards, the view's metadata becomes stale relative to the source — but the underlying buffer pointer remains valid (refcount keeps it alive). Documented invariant: re-create the view if the source changes.

#### 24. Internal classes can't have refcounted property defaults

`zend_declare_property` with an `array_init`'d zval as default produces "Internal zvals cannot be refcounted" at MINIT. The fix: declare array properties with `zend_declare_property_null` and populate at construction time via `zend_update_property`. Same applies to non-empty string defaults. Scalar defaults (long, bool, double, empty string) work fine via the `_long` / `_bool` / `_double` / `_string` variants.

## Open Items / Caveats

## Open Items / Caveats

- ~~**LAPACK symbol portability on macOS.**~~ **Fixed in Story 10.** `config.m4` now probes both `dgetri_` (Linux convention) and `dgetri` (macOS Accelerate). `lapack_names.h` aliases symbols when the no-underscore variant is in use. macOS CI lane is now blocking.
- **gcov coverage gate parked.** CI runs the gcov job as `continue-on-error: true` until Story 2 lands substantive code. Flip to blocking when `ndarray.c` has real implementation.
- **Nothing depends on `nditer.c` yet.** The empty TU is listed in `PHP_NEW_EXTENSION` so the build graph stays stable. If a header declaration is added without a definition the linker fails loud — that is the intended canary.
- **Transpose-trick zero-copy deferral.** `inv` and `det` could skip the F-contig copy via the column-major reinterpretation trick (row-major bytes interpreted as column-major = transpose; `inv(A^T) = inv(A)^T` and `det(A^T) = det(A)`). `solve`, `svd`, `eig` cannot use the trick safely. v1 ships uniform copy for simplicity; revisit if profiling shows the copy is hot.

## Sprint Outcomes

- **2026-04-28 — `project-scaffolding-architecture`** delivered the buildable extension skeleton, four registered exception classes, a CI matrix (build + valgrind + gcov + macOS non-blocking), and the seven decisions above.
- **2026-04-28 — `ndarray-struct-and-creation-api`** delivered the refcounted buffer + ndarray struct, `NDArray` class with create/free/clone handlers (clone deep-copies through strides), the full creation API (`zeros`, `ones`, `full`, `eye`, `arange`, `fromArray`), metadata accessors (`shape`/`dtype`/`size`/`ndim`), and `toArray` (pulled forward from Story 11 Phase A so contents could be asserted in phpt). 8 new phpt tests, all green; build clean at -Wall -Wextra. `NUMPHP_MAX_NDIM = 16` adopted as a static cap to avoid heap-allocating dimension arrays per-op. ArrayAccess and Countable interfaces remain deferred to Story 4 because they require offsetGet/offsetSet handlers tied to indexing semantics.
- **2026-04-28 — `indexing-and-slicing`** delivered `ArrayAccess` and `Countable` on `NDArray`, view allocator that shares the parent's refcounted buffer, `read_dimension` / `write_dimension` / `has_dimension` / `unset_dimension` / `count_elements` C handlers, the matching PHP method stubs (`offsetGet` etc.), and `NDArray::slice($start, $stop, $step = 1)` for axis-0 views with negative-index normalisation. 5 new phpt tests; `$a[$i] = $value` supports scalar (1D), scalar broadcast (nD), and shape-matched `NDArray` RHS. Negative `slice` step, multi-axis tuple slicing, and PHP-array RHS for assignment are deferred to later sprints. `offsetGet` arginfo declares `IS_MIXED` return to satisfy the `ArrayAccess::offsetGet(mixed): mixed` interface contract — without this PHP 8 emits a deprecation that breaks every phpt because phpt diff is byte-exact.
- **2026-04-28 — `broadcasting-and-elementwise-ops`** combined Stories 7 + 5 to ship the broadcast-aware nd-iterator and the four arithmetic ops at once. `numphp_nditer` walks up to 4 operands with virtual zero strides for broadcast dims; `numphp_broadcast_shape` and `numphp_promote_dtype` are now load-bearing. Operator overloading via `do_operation` covers `+ - * /` for `NDArray ⊕ NDArray`, `NDArray ⊕ scalar`, and `scalar ⊕ NDArray` (scalars are wrapped in 0-D temp arrays so the iterator handles them uniformly). Float divzero follows IEEE; int divzero throws `\DivisionByZeroError`. 6 new phpt tests, all green; build clean at -Wall -Wextra. True buffer-mutating in-place methods (`addInplace` etc.) deferred — `$a += $b` works via PHP's default compound-assignment path (creates a new array).
- **2026-04-28 — `shape-manipulation`** delivered `reshape` (with `-1` inference, contiguity-aware view-vs-copy), `transpose` (default reverse + explicit permutation; always a view), `flatten` (contiguous 1-D copy), `squeeze` / `expandDims` (size-1 dim manipulation; views), and the static `concatenate` / `stack` (copy-based, dtype-promoted across inputs). Helper `materialize_contiguous` is shared between reshape's non-contig path and any future op that needs a contiguous copy. `NUMPHP_CONCAT_MAX = 64` caps the number of inputs to `concatenate` / `stack`, stack-allocated for cache friendliness. 6 new phpt tests; all 27 tests green.
- **2026-04-28 — `blas-integration`** wired `dot`, `matmul`, `inner`, `outer` to OpenBLAS. Pure float32 inputs use the s-path (`cblas_sdot` / `cblas_sgemm` / `cblas_sger`); everything else promotes to float64 and uses the d-path. The promotion rule is "if either input is non-f32 (i.e. f64 or any int), promote both to f64" — wider than the elementwise table because BLAS has no integer routines. `ensure_contig_dtype` materialises non-contiguous or wrong-dtype inputs to fresh contiguous owners before the BLAS call. cblas.h lives at `/usr/include/x86_64-linux-gnu/cblas.h` on Ubuntu (multi-arch); GCC finds it without an explicit `-I`. Deferred: 3D+ batched matmul, native-int matmul, and strided BLAS (passing `lda` instead of copying for transposed views with unit inner stride). 4 new phpt tests; all 31 tests green.
- **2026-04-29 — `stats-and-math-functions`** delivered the full reduction surface (`sum`/`mean`/`min`/`max`/`var`/`std`/`argmin`/`argmax`) with `axis` + `keepdims`, NaN-aware variants of all eight, element-wise math (`sqrt`/`exp`/`log`/`log2`/`log10`/`abs`/`power`/`clip`/`floor`/`ceil`/`round`), and `sort`/`argsort`. `var` and `std` use Welford's online algorithm; `mean` uses pairwise sum (recursive halving with leaf 8). `ops.c` is no longer a stub — all kernels live there; `ndarray.c` keeps thin PHP_METHOD wrappers and the method table. `read_scalar_at`, `write_scalar_at`, and `materialize_contiguous` were promoted to exported `numphp_*` symbols so `ops.c` can reuse them. Sort is `qsort`-backed and unstable for equal keys (documented in decision 11). Decision 8 records the deliberate divergence from NumPy on round-half semantics — locked by a dedicated test. The `axis_zv == NULL` C-pointer trick distinguishes "argument omitted" from "explicit `null`" in `sort`/`argsort` (omitted → axis = -1, explicit null → flatten). 7 new phpt tests; all 38 tests green; build clean at -Wall -Wextra. Deferred: array-valued power exponent, multi-axis tuple reductions, median/percentile/quantile, stable sort, in-place sort, cumsum/cumprod.
- **2026-04-29 — `linear-algebra-module`** delivered the `Linalg` static class — `inv`, `det`, `solve`, `svd`, `eig`, `norm` — backed by raw LAPACK (`dgetrf` / `dgetri` / `dgesv` / `dgesdd` / `dgeev`). Decision 12 picks raw LAPACK over LAPACKE (matching NumPy); decision 13 isolates platform symbol-name differences in `lapack_names.h` and the `config.m4` probe, which now tries both `dgetri_` and `dgetri` so macOS Accelerate works — the macOS CI lane flipped from `continue-on-error: true` to blocking. Decision 14 adopts uniform F-contig copy at the LAPACK boundary; the zero-copy "transpose trick" is deferred (still works mathematically for `inv`/`det`, but the copy keeps the code uniform and correct). Decision 15 restricts `eig` to real eigenvalues in v1 (informative exception names the offending eigenvalue index); decision 16 ships only thin SVD; decision 17 substitutes Frobenius for matrix-2 norm. Layout helper: every op uses `copy_to_fcontig_*` to produce column-major scratch, calls LAPACK, then `fcontig_to_ndarray_*` to transpose-copy back. f32 dispatch uses `s*` LAPACK only when both inputs are pure f32 — same rule as Story 8 BLAS. `ensure_contig_dtype` was promoted to exported `numphp_ensure_contig_dtype`. 7 new phpt tests use asymmetric matrices specifically (e.g. `[[4,7],[2,6]]`, `[[4,2,1],[3,5,1],[1,1,3]]`) so transpose-trick bugs cannot hide behind symmetric inputs. All 45 tests green; build clean at -Wall -Wextra. Deferred: `qr`/`cholesky`/`pinv`/`lstsq`/`matrix_rank`/`slogdet`, full SVD, complex dtype, batched 3-D+ linalg, spectral matrix-2, strided LAPACK.
- **2026-04-29 — `php-arrays-and-file-io` (Story 11 Phase A)** delivered the file-I/O surface: `NDArray::save` / `load` (versioned `NUMPHP\0\1` binary format, 16-byte header with 6 reserved bytes for forward extensions) and `$a->toCsv` / `NDArray::fromCsv` (RFC-4180-ish via `php_fgetcsv` / `php_fputcsv`, single-dtype, comma-only). New `io.c` / `io.h` houses real logic; `ndarray.c` PHP_METHOD wrappers are thin. Decisions 18-21 captured: stream layer (`php_stream_open_wrapper` with flags `0` so we own the error path via `\NDArrayException`); locale-safe float formatting (`zend_strtod` for reads, `setlocale(LC_NUMERIC, "C")` save/restore around writes); versioned binary format with version-byte-first validation; `php_fgetcsv` contract gotcha (caller does NOT efree buf — function takes ownership when stream is non-NULL; cell zvals are independent zend_strings copied from internal scratch). Big-endian compile-time `#error` makes the v1 platform deferral explicit. `toArray` / `fromArray` round-trip extended in `046-` to the full dtype × ndim matrix including non-contiguous transpose views. 5 new phpt tests; all 50 tests green; build clean at -Wall -Wextra. **Phase A only — story 11 stays in backlog with Phase A annotation; Phases B (FFI BufferView) and C (Arrow IPC) ship in a follow-up sprint.** Deferred: TSV / arbitrary-delimiter CSV, per-column dtype detection, CSV header value retention, `.npy` compatibility, gzip/compression, big-endian, Phases B and C.
- **2026-04-29 — `buffer-view` (Story 11 Phase B)** delivered the FFI bridge: a final `BufferView` PHP class with read-only public properties (`int $ptr`, `string $dtype`, `array $shape`, `array $strides`, `bool $writeable`), exposed via `$a->bufferView(bool $writeable = false)`. `BufferView` holds a refcount on the source's `numphp_buffer` so the buffer outlives the source `NDArray` if the user drops the array first — proves Story 2's refcounted-buffer design pays off. New `bufferview.c` / `bufferview.h` TU; thin PHP_METHOD wrapper in `ndarray.c`. Decisions 22-24 captured: `$writeable` is advisory in v1 (existing `NUMPHP_WRITEABLE` flag is informational; enforcement deferred); BufferView is a metadata snapshot at construction time, not auto-refreshed; internal classes cannot have refcounted property defaults — array properties must default to NULL and populate at construction (caused a load-time fatal during dev). C-contiguous required (transpose views throw with a `clone`-first hint). 4 new phpt tests; the FFI round-trip test (`054-`) skips cleanly when the FFI extension isn't loaded. All 53 run tests pass; 1 skipped due to missing FFI ext on this runner; build clean at -Wall -Wextra. **Phase B only — story 11 stays in backlog with Phases A+B annotated as shipped; Phase C (Arrow IPC) remains.** Deferred: hard enforcement of `$writeable`, FFI\CData factory helper, non-C-contig sources, `__toString` polish.
