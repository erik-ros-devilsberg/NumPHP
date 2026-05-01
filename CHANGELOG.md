# Changelog

All notable changes to numphp are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.0.12] — 2026-05-01

### Changed — Story 15: project layout
- All 15 C/H source files moved from the project root to `src/`. The root now holds only the meta files a newcomer reads first (`README.md`, `CHANGELOG.md`, `LICENSE`, `CLAUDE.md`, `.gitignore`, `config.m4`).
- `config.m4` updated: source paths prefixed with `src/`; `PHP_ADD_INCLUDE([$ext_srcdir/src])` added so cross-file `#include "numphp.h"` keeps resolving.
- `git mv` used throughout so `git blame` follows.
- No behavioural change: 60 phpt + 1 FFI-skip pass, all 5 examples still match their `.expected`, doc snippet harness still green.

### Added
- `LICENSE` — BSD 3-Clause, matching the broader numerical-Python ecosystem (NumPy, SciPy, pandas, scikit-learn).
- README's "License" section updated to point at the new file (was previously a TBD placeholder).
- Bumped `PHP_NUMPHP_VERSION` to `0.0.12`.

## [0.0.11] — 2026-05-01

### Added — Story 13 Phase B: examples, snippet-as-test harness, gap-closure tests
- `examples/` directory with 5 runnable scripts: `linear-regression.php`, `kmeans.php`, `image-as-array.php`, `time-series.php`, `csv-pipeline.php`. Each is deterministic, self-contained, and has a checked-in `.expected` file. New CI job `examples` diffs each script's output against `.expected` on every PR.
- `tests/100-doc-snippets.phpt` + `tests/_helpers/snippet_runner.php` — extracts every fenced ```php block from user-facing docs, runs it, compares against the expected-output block. Catches doc-vs-implementation drift automatically.
- 6 new phpt tests (`055-…` through `060-…`) closing audit-identified gaps: DType throw paths, IndexException for axis-OOR across squeeze/expandDims/concatenate/stack, BLAS edge shapes (k=1 matmul/dot/outer), int32 sum promotion at value-overflow scale (locks decision 9), ±inf propagation through every reduction, and the `fromArray` mixed-depth segfault regression test.
- `docs/coverage-audit-2026-05-01.md` — written audit identifying every covered / newly-covered / deferred coverage gap.
- `scripts/coverage.sh` — single canonical entry point for local coverage measurement; cleans up autoconf conftest leftovers and writes reports to `coverage/` (gitignored).
- Bumped `PHP_NUMPHP_VERSION` to `0.0.11`.

### Fixed
- **`fromArray` segfault on mixed-depth siblings.** `NDArray::fromArray([[1, [99]]])` (a row containing a scalar followed by a nested array) crashed the PHP process. The rank-inference walk allowed a sibling array to extend ndim past the leaf depth a previous scalar had locked, and the subsequent fill walk dereferenced the scalar as a HashTable. Fix: `rank_locked` flag in `fromarray_walk` blocks any later array at depth `>= ndim_out` once a scalar has appeared at that depth. Now throws `\ShapeException("Ragged array: array at leaf depth")`. Found by the audit, locked by `tests/060-fromarray-mixed-depth.phpt`.

### Changed
- `.github/workflows/ci.yml` — added `examples` job; widened gcov filter from `ndarray.c + ops.c` only to all 7 C sources actually shipped by `config.m4` (so the reported number reflects the real surface). Coverage job stays `continue-on-error: true`; flipping to blocking is out of scope for this sprint.
- `docs/api/ndarray.md` — corrected `full()` and `fromArray()` throws-list. Previously claimed `\DTypeException` for non-numeric values and NaN-cast-to-int; neither is true (the C silently coerces via PHP's standard cast, matching NumPy on NaN→int).

### Notes — what this sprint was for
Phase B is the "does the API survive realistic use" pass. Examples are the honest stress test — they exercise APIs in ways the 54 phpt tests didn't, and one of them (the `fromArray` segfault) surfaced a bug. The snippet harness is regression insurance for documentation: 75 fenced code blocks across user-facing docs now run on every PR. Phase C (benchmarks) and the eventual coverage-gate flip stay deferred — they belong to the v0.1.0 release-quality push, not the iterative pre-release we're in.

## [0.0.10] — 2026-04-29

### Added — Story 11 Phase B: FFI BufferView
- New `BufferView` class. Final, no public constructor — instances come only from `NDArray::bufferView()`. Public read-only properties: `int $ptr` (the buffer's address as `uintptr_t`), `string $dtype`, `array $shape`, `array $strides`, `bool $writeable`.
- `$a->bufferView(bool $writeable = false): BufferView` — returns a view that holds a refcount on the underlying buffer.
- 4 new phpt tests (`051-…` through `054-…`); the FFI round-trip test (`054-`) skips cleanly when the `FFI` extension isn't loaded.
- Bumped `PHP_NUMPHP_VERSION` to `0.0.10`.

### Notes — refcount-protected lifetime
`BufferView` bumps the underlying `numphp_buffer`'s refcount on construction and decrements on destruction. **The buffer outlives the source `NDArray` if the user drops the array first.** This is the property the FFI consumer needs: hold a `BufferView` and the pointer stays valid. Story 2 designed `numphp_buffer` for exactly this — Phase B just wires the existing refcount machinery to a new holder type.

### Notes — `$writeable` is advisory in v1
We expose `$writeable` to the consumer as a contract bool, but do not enforce read-only on the source `NDArray`. Reasoning:
1. The existing `NUMPHP_WRITEABLE` flag is already informational; no code path enforces it.
2. Adding enforcement everywhere is scope creep.
3. The FFI consumer is the trust boundary; documenting the contract has the same practical effect as enforcement.

### Notes — C-contiguous required
`bufferView()` throws `\NDArrayException` if the source isn't C-contiguous (e.g. a `transpose()` view). Workaround: `clone $arr` (deep-copies into a fresh C-contig owner) before calling `bufferView()`. This avoids handing FFI consumers a buffer they would walk linearly and get wrong values from.

### Notes — `$ptr` interpretation
`$ptr` is the buffer's address as a `uintptr_t` cast to PHP `int` (which is platform-native long — 64-bit on the supported 64-bit platforms). Consumers using PHP's `FFI` extension can build a `CData` pointer from it via the standard FFI patterns. Documenting one-liner recipes is a Story 13 (docs) task.

### Refactor
- New `bufferview.c` / `bufferview.h` translation unit. Real BufferView logic lives there; `NDArray::bufferView()` PHP_METHOD in `ndarray.c` is a thin wrapper around `numphp_bufferview_create`.
- `config.m4` `PHP_NEW_EXTENSION` now lists `bufferview.c` in the source set.
- `MINIT` registers `BufferView` after `Linalg`.

### Deferred
- Enforcement of `$writeable` (would require WRITEABLE-flag honoring throughout `ndarray.c`).
- Strides for non-C-contig sources (current restriction simplifies consumers).
- A helper that returns a ready-made `FFI\CData` instead of an int address.
- `BufferView::__toString()` showing metadata in human-readable form.
- Phase C (Arrow IPC) — separate sprint.

## [0.0.9] — 2026-04-29

### Added — Story 11 Phase A: PHP arrays & file I/O
- `NDArray::save(string $path): void` — writes a versioned little-endian binary file (magic `"NUMPHP\0\1"`, dtype byte, ndim byte, 6 reserved bytes, int64 shape, raw buffer). Non-contiguous inputs are materialised C-contig before write.
- `NDArray::load(string $path): NDArray` (static) — reads files written by `save`. **Loader checks the format-version byte first** and throws a specific error (`"NUMPHP binary format version N is not supported; this build understands version 1. Re-save with an older numphp or upgrade."`) if the byte is unknown.
- `$a->toCsv(string $path): void` — writes 1-D as one cell per row, 2-D as comma-separated rows. ndim ≥ 3 → `\ShapeException`. Floats are written via `%.17g` for bit-exact round-trip.
- `NDArray::fromCsv(string $path, string $dtype = 'float64', bool $header = false): NDArray` (static) — returns a 2-D array. UTF-8 BOM is auto-skipped. Single-column input produces shape `(n, 1)`.
- 5 new phpt tests (`046-…` through `050-…`). 50/50 tests now passing.

### Notes — locale-safe float formatting
Both reader and writer are protected against the user's PHP `LC_NUMERIC` setting. The reader uses `zend_strtod` (locale-independent — always accepts `.` decimal). The writer brackets its inner loop with `setlocale(LC_NUMERIC, "C")` save/restore so `1.5` is always written as `1.5`, never `1,5`. Without these, a user with German locale would produce CSVs that even our own reader couldn't parse.

### Notes — stream layer
File I/O goes through `php_stream_open_wrapper` instead of plain `fopen`, so `open_basedir` and stream wrappers (`phar://`, `data://`, custom user wrappers) are honoured automatically. Eliminates the path-traversal / sandboxing footgun that would otherwise be a documented limitation.

### Notes — binary format extensibility
The 16-byte header reserves 6 bytes (offsets 10..15) for forward extensions. Bumping the format version byte is reserved for breaking changes only; loader/saver share `#define NUMPHP_BINARY_FORMAT_VERSION 1` from `io.h` so they can't drift.

### Notes — Phase A scope only
Story 11 has three phases. Phase A (this release) ships PHP arrays and file I/O. Phases B (FFI `BufferView`) and C (Arrow IPC) remain in the backlog as a follow-up sprint.

### Refactor
- New `io.c` / `io.h` translation unit. Real I/O logic lives there; `ndarray.c` PHP_METHOD wrappers are thin.
- Bumped `PHP_NUMPHP_VERSION` to `0.0.9`.
- `config.m4` `PHP_NEW_EXTENSION` now lists `io.c` in the source set.

### Deferred
- TSV / pipe-delimited / arbitrary-delimiter CSV (no `delimiter` argument yet).
- Per-column dtype detection in CSV reader.
- CSV header value retention (no `columns()` accessor).
- NumPy `.npy` / `.npz` format compatibility — out of scope; we have our own `NUMPHP\0\1` format. Cross-language interop will live in Phase C (Arrow).
- Stream wrappers other than file:// can still be used via `php_stream_open_wrapper`, but `phar://` and similar haven't been exhaustively tested.
- gzip / compression on save/load.
- Big-endian platforms — compile-time `#error` makes the deferral explicit.
- Phase B (`BufferView` for FFI) and Phase C (Arrow IPC) — separate sprint.

## [0.0.8] — 2026-04-29

### Added
- **`Linalg` static class.** Six methods backed by raw LAPACK:
  - `Linalg::inv($a): NDArray` — matrix inverse via `dgetrf` + `dgetri`.
  - `Linalg::det($a): float` — determinant via `dgetrf` (product of U diagonal × pivot sign).
  - `Linalg::solve($a, $b): NDArray` — `Ax = b` via `dgesv`. `$b` may be 1-D (vector) or 2-D (multiple RHS).
  - `Linalg::svd($a): array` — thin SVD `[U, S, Vt]` via `dgesdd` (`JOBZ='S'`).
  - `Linalg::eig($a): array` — eigenvalues + right eigenvectors `[w, V]` via `dgeev`.
  - `Linalg::norm($a, $ord = 2, $axis = null): float|NDArray` — vector norms (2, 1, INF) and matrix norms (Frobenius/'fro', 1, INF). Optional `$axis` for vector norms across rows/cols of a matrix.
- f32 inputs run on the s-path (`sgetrf`, `sgesv`, `sgesdd`, `sgeev`); everything else (f64, mixed, integer) promotes to f64 and runs on the d-path. Same dispatch rule as Story 8 BLAS.
- macOS LAPACK CI lane is now blocking. `config.m4` probes both `dgetri_` (Linux convention, with trailing underscore) and `dgetri` (macOS Accelerate convention, no underscore); on a successful no-underscore probe it defines `NUMPHP_LAPACK_NO_USCORE` so `lapack_names.h` aliases the symbols.
- New `lapack_names.h` — single source of truth for all LAPACK symbol names. Adding a new routine = one line in this header.
- 7 new phpt tests (`039-…` through `045-…`) covering each op + dtype dispatch + non-contiguous input.

### Notes — eigenvalues are real-only in v1
`Linalg::eig` throws `\NDArrayException` if the input has complex eigenvalues. The exception message names the offending eigenvalue index and its imaginary magnitude. Workaround: ensure the input is symmetric (`A == A^T`) — guaranteed real eigenvalues. Complex dtype lands in v2.

### Notes — SVD is thin only in v1
`Linalg::svd` always returns the thin (economy) SVD: `U` is `(m, k)`, `Vt` is `(k, n)`, where `k = min(m, n)`. The full SVD (`JOBZ='A'`, with `U` as `(m, m)` and `Vt` as `(n, n)`) is deferred — most users want the thin form, and matching NumPy's `full_matrices = True` keyword can come later without breaking changes.

### Notes — matrix-2 norm
`Linalg::norm($matrix, 2)` returns the Frobenius norm in v1 instead of the spectral (largest singular value) norm. Document the divergence; the spectral norm requires running an SVD on every call, which is expensive — defer until a user needs it. Vector-2 norm is unaffected (Euclidean).

### Refactor
- `linalg.c` is no longer a stub. All six ops live there as PHP_METHODs + per-dtype kernels.
- Promoted `ensure_contig_dtype` to an exported `numphp_ensure_contig_dtype` symbol so `linalg.c` can use it.
- Bumped `PHP_NUMPHP_VERSION` to `0.0.8`.

### Layout strategy
Every linalg op materialises 2-D inputs into an F-contiguous (column-major) scratch buffer of the right dtype, hands that to LAPACK, then transpose-copies the result back into a row-major `NDArray`. The "transpose trick" (interpret row-major bytes as column-major to skip the copy) works for `inv` and `det` but breaks for multi-RHS `solve`, `svd`, and `eig` — uniform col-major copy keeps the code simple and correct. Performance optimisation (zero-copy fast path on `inv`/`det`, strided LAPACK with `LDA`) deferred.

### Deferred
- `qr`, `cholesky`, `pinv`, `lstsq`, `matrix_rank`, `slogdet`.
- Full SVD (`full_matrices = true`).
- Complex eigenvalues / complex dtype generally.
- `norm` ords `-1`, `-2`, `-inf`, nuclear norm.
- 3-D+ batched linalg (NumPy's "stack of matrices" semantics).
- Spectral matrix-2 norm (largest singular value).
- Strided LAPACK (passing `LDA` for views with unit inner stride).

## [0.0.7] — 2026-04-29

### Added
- **Reductions** with `axis` and `keepdims`: `sum`, `mean`, `min`, `max`, `var`, `std`, `argmin`, `argmax`. All instance methods on `NDArray`. Pass `axis = null` (or omit) for a global reduction; pass an int (negative allowed) to collapse one axis. `keepdims = true` preserves the reduced axis as size 1 for broadcast-back.
- **Numerical stability:** `mean` uses pairwise sum (recursive halving with leaf 8). `var` and `std` use **Welford's online algorithm** — single pass, no catastrophic cancellation. `var($axis, $keepdims, $ddof)` accepts a Bessel correction (`ddof = 1` for sample variance).
- **NaN-aware variants:** `nansum`, `nanmean`, `nanmin`, `nanmax`, `nanvar`, `nanstd`, `nanargmin`, `nanargmax`. Skip NaN inputs in float dtypes; alias the regular form for integer dtypes. All-NaN slice → `nansum` returns 0, `nanmean`/`nanmin`/`nanmax` return NaN, `nanargmin`/`nanargmax` throw `\NDArrayException`.
- **Element-wise math:** `sqrt`, `exp`, `log`, `log2`, `log10`, `abs`, `power($exp)`, `clip(?float $min, ?float $max)`, `floor`, `ceil`, `round(int $decimals = 0)`. Domain rules follow IEEE 754 (`sqrt(-x)` = NaN, `log(0)` = -inf), no exception. Output dtype: `sqrt` preserves float dtype, integer → f64; `exp`/`log*` always f64; `abs`/`floor`/`ceil`/`clip` preserve dtype.
- **Sort:** `sort(?int $axis = -1)` and `argsort(?int $axis = -1)`. Default sorts the last axis; explicit `null` flattens then sorts. Negative axis allowed. Backed by `qsort` (unstable for equal keys — distinct-value tests lock the documented behavior). `argsort` always returns int64.
- **Output dtype rules** for reductions: `sum` of int → int64; `mean`/`var`/`std` of int → float64; `min`/`max` preserve dtype; `argmin`/`argmax` always int64.
- 7 new phpt tests (`032-…` through `038-…`) covering reductions, NaN-variants, math ops, sort/argsort, edge cases.

### Notes — round-half semantics (deliberate divergence from NumPy)

`NDArray::round()` rounds **half-away-from-zero** (PHP's default `round()` mode, `PHP_ROUND_HALF_UP`). This matches the PHP scalar `round()` and the common user expectation that `round(0.5) === 1.0`.

NumPy's `np.round` uses banker's rounding (half-to-even): `np.round(0.5)` is `0.0`, `np.round(1.5)` is `2.0`. **numphp does not.** `[0.5, 1.5, 2.5, -0.5, -1.5]->round() === [1.0, 2.0, 3.0, -1.0, -2.0]`. This is locked by `tests/036-elementwise-math.phpt`. See `docs/system.md` Architectural Decisions for the full rationale.

### Refactor
- `ops.c` is no longer a stub. All reduction kernels, element-wise math, and sort live there. Methods in `ndarray.c` are thin PHP_METHOD wrappers around the kernels.
- Promoted `read_scalar_at`, `write_scalar_at`, and `materialize_contiguous` to exported symbols (`numphp_read_scalar_at`, `numphp_write_scalar_at`, `numphp_materialize_contiguous`) so kernels in `ops.c` can use them.

### Deferred
- `median`, `percentile`, `quantile`, `cumsum`, `cumprod`.
- Multi-axis tuple reductions (`sum(axis=(0, 2))`) — chain `sum(0)->sum(0)`.
- Stable sort guarantee.
- In-place sort.
- Array-valued exponent in `power($a)` (currently scalar exponent only).

## [0.0.6] — 2026-04-28

### Added
- `NDArray::dot($a, $b)` — 1-D inner product via `cblas_ddot` / `cblas_sdot`. Returns PHP `float`.
- `NDArray::matmul($a, $b)` — 2-D matrix multiply via `cblas_dgemm` / `cblas_sgemm`.
- `NDArray::inner($a, $b)` — 1-D alias for dot semantics; nD inputs throw with a "use matmul" pointer.
- `NDArray::outer($a, $b)` — 1-D × 1-D → 2-D rank-1 update via `cblas_dger` / `cblas_sger`.
- Pure float32 inputs run on the s-path; everything else (float64, mixed, integer) promotes to float64 and runs the d-path.
- Non-contiguous inputs are materialised contiguously before the BLAS call (`ensure_contig_dtype` helper).

### Deferred
- 3D+ batched matmul.
- Native int matmul (currently promotes to float64).
- Strided BLAS (passing `lda` for views with unit inner stride) — saves the copy on transposed inputs.

## [0.0.5] — 2026-04-28

### Added
- `$a->reshape($shape)` with `-1` placeholder inference; returns a view when the source is C-contiguous, else a contiguous copy.
- `$a->transpose($axes = null)` reverses axes by default or applies an explicit permutation. Always a view.
- `$a->flatten()` returns a 1-D C-contiguous copy.
- `$a->squeeze($axis = null)` drops size-1 dims; named non-1 axis throws `\ShapeException`.
- `$a->expandDims($axis)` inserts a size-1 dim with stride 0 (broadcast-friendly).
- `NDArray::concatenate($arrays, $axis = 0)` static; joins along an existing axis with cross-input dtype promotion.
- `NDArray::stack($arrays, $axis = 0)` static; adds a new axis; all input shapes must match exactly.
- 6 new phpt tests covering each operation, edge cases, and error paths.

## [0.0.4] — 2026-04-28

### Added
- `numphp_nditer` — broadcast-aware nd-iterator with up to 4 operands, used as the engine for element-wise ops and (later) reductions and BLAS dispatch.
- `numphp_broadcast_shape` and `numphp_promote_dtype` helpers; promotion table from `docs/system.md` is now executable.
- `NDArray::add`, `subtract`, `multiply`, `divide` static methods. Operands may be `NDArray` or scalar.
- Operator overloading for `+ - * /` via `do_operation`. `$a + $b` works for `NDArray + NDArray`, `NDArray + scalar`, and `scalar + NDArray`. Compound `$a += $b` composes via PHP's default behavior.
- Float division by zero follows IEEE 754; int division by zero throws `\DivisionByZeroError`.
- Incompatible broadcast shapes throw `\ShapeException` with the offending axis.
- 6 new phpt tests (matching-shape arithmetic, broadcasting, operator overload, dtype promotion, divzero, shape mismatch).

### Deferred
- True buffer-mutating in-place methods (`$a->addInplace($b)`).

## [0.0.3] — 2026-04-28

### Added
- `ArrayAccess` and `Countable` interfaces on `NDArray`.
- `$a[$i]` / `$a->offsetGet($i)` returns scalar (1D) or view (nD), with negative indexing.
- `$a[$i] = $value` supports scalar (1D), scalar broadcast (nD), and shape-matched `NDArray` RHS.
- `count($a)` and `$a->count()` return total elements.
- `NDArray::slice($start, $stop, $step = 1)` returns an axis-0 view.
- Views share the parent's refcounted buffer; mutations through views propagate; cloning a view deep-copies.
- 5 new phpt tests covering offsetGet, offsetSet, Countable, slice, and view-sharing semantics.

### Deferred
- Negative `step` in `slice` and multi-axis tuple slicing (Story 6+ / Story 7).
- PHP-array RHS in `$a[$i] = […]` (waits on Story 7 broadcasting).

## [0.0.2] — 2026-04-28

### Added
- `numphp_buffer` (refcounted data buffer) and `numphp_ndarray` (metadata shell) structs.
- `NDArray` class with create / free / clone object handlers; clone deep-copies through strides.
- Static creation API: `zeros`, `ones`, `full`, `eye`, `arange`, `fromArray`.
- Instance metadata accessors: `shape`, `dtype`, `size`, `ndim`.
- `toArray` (recursive PHP-array reconstruction) — pulled forward from Story 11.
- 8 phpt tests covering each creation method, dtype inference, ragged rejection, metadata, and `toArray`/clone round-trip.

## [0.0.1] — 2026-04-28

### Added
- Project skeleton: `config.m4`, module entry, `MINIT`, `MINFO`.
- Exception class hierarchy: `NDArrayException` (extends `\RuntimeException`),
  `ShapeException`, `DTypeException`, `IndexException` — all registered in `MINIT`.
- Header stubs and empty translation units for `ndarray`, `ops`, `linalg`, `nditer`
  so the build graph is stable across upcoming stories.
- Test infrastructure: phpt files for extension load and exception registration.
- CI: GitHub Actions matrix over PHP 8.2 / 8.3 / 8.4 on Linux, with valgrind,
  gcov coverage, and macOS as non-blocking lanes.
- Cross-cutting decisions captured in `docs/system.md`.
