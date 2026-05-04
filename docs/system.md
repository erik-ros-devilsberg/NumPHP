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

### 2026-05-01 — Sprint: Documentation Pass (Story 13 Phase A)

#### 25. Markdown for the API reference (not DocBook)

The reference at `docs/api/` is plain Markdown with a strict per-method template (signature heading → params table → returns → throws → example). The php.net docs use XML/DocBook, but bringing that toolchain into the repo is overkill for a single-extension reference, and Markdown renders directly on GitHub / VS Code / mkdocs without conversion. Format consistency is enforced by writing the template into `docs/api/README.md` itself and producing one exemplar entry (`NDArray::zeros`) before drafting the remaining entries against it. Format-validation tooling (e.g. CI that grep-checks for missing sections) is deferred — manual proofreading was sufficient for v1's surface size.

#### 26. Naming convention: `numphp` in code, `NumPHP` in prose

Two names appear across the project. Lock-in to avoid drift:

- **`numphp`** (lowercase) — code, install commands, CLI flags, file names, `extension=numphp.so`, the C struct prefix, the user-story filename slugs.
- **`NumPHP`** (mixed case) — prose in docs, headlines, the project's name when referenced as a noun, the README title.

Rationale: the lowercase form mirrors the C extension naming convention (`mysqli`, `intl`, `gd`); the mixed-case form is the brand most readers will see first in marketing material and matches the epic title.

#### 27. `count()` returns total element count (diverges from NumPy)

`Countable::count()` returns the array's total element count (same as `size()`), not the leading-axis size. This was implemented in Story 4 and locked by `tests/013-countable.phpt` (`count(zeros([3,4])) === 12`) but never written down as a deliberate divergence. Surfaced during the documentation pass when a snippet asserting `count(zeros([4,7]) === 4)` failed against the actual `28`. NumPy's `len(a)` returns `shape[0]`; PHP's `Countable` contract says "the count of items," and NumPHP interprets that literally. To get the leading-axis size, use `shape()[0]`. Documented prominently in `docs/api/ndarray.md` and `docs/cheatsheet-numpy.md` so users porting from NumPy aren't surprised.

### 2026-05-01 — Sprint: Project Layout (Story 15)

#### 28. C sources live under `src/`, not at the project root

The 15 tracked C/H files moved from the root to `src/`. The root is now ~6 tracked files — meta a newcomer reads first (`README`, `CHANGELOG`, `LICENSE`, `CLAUDE.md`, `.gitignore`, `config.m4`). `config.m4`'s `PHP_NEW_EXTENSION` invocation lists `src/foo.c` paths and adds `PHP_ADD_INCLUDE([$ext_srcdir/src])` so cross-file `#include "numphp.h"` keeps resolving.

The alternative (PECL-canonical flat root) is what most ext/ in php-src and most PECL submissions ship. The departure is small — `package.xml` (when added in Story 12) accepts paths — but visually makes the repo's intent legible: the root is for meta files, the sources live in `src/`.

#### 29. License — BSD 3-Clause

Matches the wider numerical-Python ecosystem (NumPy, SciPy, pandas, scikit-learn). PHP-license is deprecated for new code; MIT and BSD-3-Clause are both popular for third-party PHP extensions; BSD-3 wins on lineage — anyone porting Python data work knows what BSD-3 means and won't audit it. `LICENSE` at the repo root; README's license section points there.

### 2026-05-01 — Sprint: Benchmarks (Story 13 Phase C)

#### 30. Benchmark methodology — locked

Every benchmark numphp publishes must follow this protocol so that
re-runs and cross-machine comparisons are honest:

- **Engines:** numphp (`hrtime(true)`) and NumPy
  (`time.perf_counter_ns()`). Both monotonic, nanosecond-precision.
- **Runs per scenario:** 7. Drop the slowest 1 (cold-cache outlier).
  Report median, min, max of the surviving 6.
- **Warm-up:** one untimed run per scenario.
- **Fixtures:** built once before the timer starts. Per-scenario timing
  covers the operation only — except for `fromArray` / `toArray`
  scenarios, where the fixture *is* the subject.
- **Determinism:** `mt_srand(42)` / `np.random.seed(42)`. Identical
  synthetic data across runs and across engines.
- **Per-run printed metadata:** hardware fingerprint
  (`uname -srm`, CPU model, MHz, count, RAM total, distro), BLAS
  variant for both engines (`ldd modules/numphp.so | grep blas/lapack`
  for numphp; `np.show_config(mode='dicts')` for NumPy), PHP and NumPy
  version.
- **Output format:** Markdown table to stdout (humans), JSONL to
  `bench/last-run/{numphp,numpy}.jsonl` (machines / future regression
  detection).
- **Single source of truth for scenarios:** `bench/scenarios.json`.
  Both `run.php` and `run.py` read this file. Adding a scenario means
  editing one file; if the new id matches an existing prefix, no
  runner code change needed.
- **Where benchmarks run:** locally only. Not in CI — shared runners
  are too noisy for meaningful timing.

The first run committed as `docs/benchmarks.md` is dated and
hardware-tagged. Future runs on better hardware should append a new
section, not overwrite the old one — the project's thesis benefits
from a record of how the numbers move over time.

### 2026-05-01 — Sprint: Bool, comparisons, and where (Story 17)

#### 32. bool dtype: canonical 0/1, lenient read, strict write

`NUMPHP_BOOL = 4` is stored as 1 byte per element. **Writes canonicalise** to `0` or `1`: `numphp_write_scalar_at` for bool stores `(lv != 0 || dv != 0.0) ? 1 : 0`. **Reads accept any non-zero byte as true**: `numphp_read_f64` / `read_i64` for bool return `(byte != 0) ? 1 : 0`.

This makes **`bool + bool` arithmetic effectively logical OR** — `[true] + [true]` reads as `1 + 1 = 2`, then write-canonicalisation turns the `2` back into `1` (true). Matches NumPy: `np.array([True]) + np.array([True])` → `[True]`. Subtraction becomes XOR; multiplication becomes AND.

**Why** (alternative considered: "promote bool arithmetic internally to int64"): the promotion table locks `bool + bool = bool`. Internal int64 promotion would either silently widen the output dtype (violating the table) or require a separate arithmetic-only promotion table (premature abstraction). Read-lenience + write-canonicalisation keeps the promotion table consistent with no extra dispatch.

**Implementation note:** the sprint-16 element-wise fast path writes through typed pointers and would bypass `numphp_write_scalar_at`. The fast-path predicate explicitly excludes `out_dt == NUMPHP_BOOL` so bool arithmetic always takes the slow path, where the canonicalising switch handles it. One unreachable `case NUMPHP_BOOL: break;` in the fast-path switch silences `-Wswitch`.

#### 33. Comparison NaN policy: IEEE 754 (story wording corrected)

Element-wise comparisons follow IEEE 754, which NumPy follows:

| Op | Either operand is NaN |
|----|------------------------|
| `eq` | `false` |
| `ne` | **`true`** |
| `lt`, `le`, `gt`, `ge` | `false` |

The story 17 wording said "any NaN operand makes eq / ne / lt / le / gt / ge return false." That contradicts both IEEE 754 (NaN ≠ NaN is `true`) and NumPy (`np.not_equal(np.nan, np.nan) == True`). The decision corrects to IEEE/NumPy semantics. Locked by `tests/065-comparisons.phpt`, which asserts `NDArray::ne(NAN, NAN) === true` — would fail under the literal story wording.

The kernel uses two paths: the float path checks NaN explicitly; the integer/bool path skips the check entirely (no NaN possible).

#### 34. Bool through reductions follows int promotion

Reduction output dtype rules extended to cover `bool` input — the reduction kernels treat bool as if it were below int32:

| Op | bool input |
|----|-----------|
| `sum`, `cumsum`, `cumprod` | `int64` (counts trues; products of bools) |
| `mean`, `var`, `std` | `float64` |
| `min`, `max` | `bool` (preserved) |
| `argmin`, `argmax` | `int64` (always) |

Implementation is one-line additions in `reduce_out_dtype` and `cumulative_out_dtype` — `if (in == NUMPHP_BOOL || in == NUMPHP_INT32 || in == NUMPHP_INT64) return NUMPHP_INT64;` for sum-family; same shape for mean-family. min/max keep their preserve-input branch and pick up bool naturally. Justification: bool sits at the bottom of the promotion table — applying the same "int → wider int" logic that decisions 9 and 31 encode is the only consistent extension.

#### 35. Comparison ops are method-only in v1

`==` / `!=` / `<` / `>` / `<=` / `>=` operators on NDArray remain unoverloaded. Zend Engine's comparison hook (`zend_class_entry->compare`) returns a single int ordering, which doesn't fit "return an array of element-wise results." The clean way to overload comparison-as-array would require a non-trivial detour through `do_operation`-style hooks that don't have a sanctioned comparison cousin. The method API (`NDArray::gt($a, $b)`) keeps the syntax explicit and unambiguous.

Revisit if a clean Zend hook surfaces or v2 breaks compatibility intentionally. Story 17 originally locked this; restating as a decision so it's discoverable from the cumulative system record rather than buried in the sprint plan.

### 2026-05-01 — Sprint: Cumulative reductions (Story 18)

#### 31. Product reductions on int promote to int64 — locked divergence from NumPy

`NDArray::cumprod`, `cumprod`, `prod`, and `nanprod` on
`int32` / `int64` / `bool` input return an `int64` scalar (or `int64`
NDArray for axis reductions). NumPy preserves the input dtype
(`int32` in → `int32` out) for `prod` and `cumprod` specifically,
even though `np.cumsum` already promotes to a wider int. NumPHP
picks consistency over NumPy parity here:

| Input | `sum` | `cumsum` | `cumprod` | `prod` |
|---|---|---|---|---|
| `bool` | `int64` | `int64` | `int64` | `int64` |
| `int32` | `int64` | `int64` | `int64` (NumPy: `int32`) | `int64` (NumPy: `int32`) |
| `int64` | `int64` | `int64` | `int64` | `int64` |
| `float32` | `f32` | `f32` | `f32` | `f32` |
| `float64` | `f64` | `f64` | `f64` | `f64` |

**Why:** the silent-overflow concern that drove decision 9 for
`sum` applies — arguably *more* — to `cumprod` and `prod`, where
products grow faster than sums. Returning the input dtype on
`int32` would silently overflow on the third multiplication of a
modestly-sized array. The cost of divergence is paid once at
documentation time; the cost of NumPy parity would be paid every
time a user's pipeline silently produces a corrupted product.

**How to apply:** kernel always allocates an `int64` output for
integer cumulative-product *and* product ops
(`cumulative_out_dtype` and `reduce_out_dtype` in `ops.c`).
Documented in `docs/api/ndarray.md` (cumprod / prod entries),
`docs/concepts/dtypes.md` (reductions table footnote), and
flagged with **Diverges** in the NumPy cheatsheet. Locked by
`tests/062-cumulative.phpt` (cumprod) and
`tests/065-prod-nanprod-countnonzero-ptp.phpt` (prod, including
an explicit overflow-scale assertion: `[100000, 100000, 100000]`
of `int32` returns `1_000_000_000_000_000` — would wrap on
`int32`, fits comfortably in `int64`).

**Sprint 20a amendment** (2026-05-04): `prod` / `nanprod` added.
Decision number 39 was reserved in the sprint plan but folded
into 31 instead — the rationale is identical and a separate
number would fragment the policy across two decisions.

### 2026-05-03 — Sprint: Build Quality Hardening — Compiler Flags (Story 19, Phase A)

#### 36. Canonical compiler warning flag set

NumPHP builds with:

```
-Wall -Wextra -Werror -Wshadow -Wstrict-prototypes -Wmissing-prototypes
```

(plus `-O2 -g` for release CI, `-O0 -g` for valgrind / coverage / sanitizer
builds.) Applied uniformly across every CI job (`build-test` matrix,
`examples`, `valgrind`, `macos`) and documented as the local build
expectation in `docs/RESUME.md`.

**Why:** the prior `-Wall -Wextra` baseline accepted warnings (1
`-Wunused-parameter` was firing silently on `main`); without `-Werror`
new warnings could accumulate without surfacing. The four added
warning flags catch real bug shapes — shadowed locals (`-Wshadow`), bare
`()` argument lists (`-Wstrict-prototypes`), undeclared cross-TU
helpers (`-Wmissing-prototypes`) — at no false-positive cost in our
codebase. `-Werror` makes the build fail-fast on regression.

**Excluded deliberately:**

- `-Wpedantic` — Zend macros (`PHP_FUNCTION`, arginfo, method tables)
  expand to non-ISO-C constructs we can't fix; suppressing per-include
  is more mess than it's worth on a PHP extension.
- `-Wconversion` — too noisy for numerical code (every
  `size_t ↔ int` and integer narrowing fires).
- `-Wcast-align`, `-Wnull-dereference`, `-Wdouble-promotion` —
  deferred to a future sprint. Each likely requires a project-wide
  source-pattern decision (alignment policy / null-check policy /
  f32 literal suffix policy) that fits its own focused sprint.

**Per-TU suppressions accepted:** `#pragma GCC diagnostic ignored
"-Wmissing-prototypes"` wraps the file-scope regions of
`src/numphp.c`, `src/ndarray.c`, `src/linalg.c`, `src/bufferview.c`
that contain Zend-macro-generated functions (`PHP_METHOD`,
`PHP_MINIT_FUNCTION`, `PHP_MINFO_FUNCTION`, `ZEND_GET_MODULE`).
These functions are referenced only via the `zend_function_entry`
table by function pointer; we can't make them `static` (the table
takes their address) and they don't belong in a header (each is
internal to its TU). The pragma is file-scoped rather than
per-function to keep diff churn low — risk: a future bare
non-static helper in the same TU would slip through. Acceptable
given we have none today and code review catches it.

**Coverage job exemption:** the `coverage` CI job runs with the
warning flags but **without** `-Werror` because `--coverage`
instrumentation can introduce diagnostics that don't reflect source
defects. The job is `continue-on-error: true` anyway (informational,
not blocking).

**How to apply:** keep this flag set in every `make CFLAGS=...` call
(local + CI). When adding new files or refactoring, prefer making
helpers `static` over adding to a header. The pragma blocks should
remain narrowly scoped to the Zend-macro regions.

### 2026-05-03 — Sprint: Build Quality Hardening — ASan + UBSan (Story 19, Phase B)

#### 37. Sanitizer policy: ASan + UBSan in CI, leak detection deferred

NumPHP runs the phpt suite under
`-fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g` in
a dedicated CI job (`sanitizers`) on every push and PR. Compiler
is gcc (system default) — clang's "reference compiler" advantage
is cosmetic at our scale and dropping it removes a CI install
step. `LD_PRELOAD` ensures libasan + libubsan load before libc
malloc (the extension is `dlopen`'d into PHP after startup, so
without preload ASan complains "runtime does not come first in
initial library list"). `USE_ZEND_ALLOC=0` disables PHP's pool
allocator so ASan can see every alloc.

**Leak detection (LSan) is ON** via
`ASAN_OPTIONS=detect_leaks=1` plus
`LSAN_OPTIONS=suppressions=lsan.supp`. The suppression file at
the repo root masks PHP-internal startup leaks (`getaddrinfo`,
`dlopen` `_init` constructors, `zend_startup_module_ex`-driven
module-table allocations) — PHP intentionally skips final
cleanup at process shutdown for CLI speed; these aren't our
code. Every entry in `lsan.supp` carries a one-line `#` comment
explaining the rationale, and the file rejects any suppression
on a numphp symbol.

ASan, UBSan, and LSan together catch heap/stack overflows,
use-after-free, double-free, signed integer overflow, alignment
violations, shifts past type width, refcount leaks (now that
`do_operation`'s compound-assign bug is fixed — see sprint
19b-fix), and other UB.

**Local convenience:**

- `scripts/sanitize.sh` — full local sanitizer build + phpt run.
  Mirrors the CI job exactly. Recommended when active work
  touches refcounts, buffer arithmetic, or error paths.
- `scripts/memcheck.sh` — full local valgrind run via
  `run-tests.php -m`. Fails fast if valgrind isn't installed
  (prints `sudo apt install valgrind`). Recommended at sprint
  boundaries — too slow for every-build use.

Neither script is a mandatory toolchain — CI is the gate.

**How to apply:** new code that touches buffer pointer arithmetic
or zval refcounting should be exercised through `scripts/sanitize.sh`
locally before push. The existing valgrind CI job (`-O0 -g` build,
`TEST_PHP_ARGS=-m`) stays in place — different bug-shapes, no
overlap with ASan worth eliminating.

### 2026-05-04 — Sprint: Build Quality Hardening — Debug PHP + ZEND_RC_DEBUG (Story 19, Phase C)

#### 38. Debug-PHP CI policy: refcount-protocol + engine-assertion gate

NumPHP runs the phpt suite + examples diff against a
debug-built PHP in a dedicated CI job (`debug-php`) on every
push and PR. The PHP binary is built from source with
`--enable-debug` (which gates `ZEND_DEBUG`, `ZEND_RC_DEBUG`,
`ZEND_ALLOC_DEBUG`, and the engine's `ZEND_ASSERT` suite),
cached in `/opt/php-debug` keyed by PHP version, and put on
`PATH` ahead of the runner default. Test step env:

```
ZEND_RC_DEBUG=1
ZEND_ALLOC_DEBUG=1
USE_ZEND_ALLOC=0
```

**What this catches that ASan / UBSan / valgrind don't:**

- **Refcount leaks that aren't memory leaks** — a forgotten
  `zval_ptr_dtor` on a return path. ASan/valgrind say "freed
  at shutdown, fine." Debug PHP says "leaked refcount" with a
  trace.
- **Refcount underflows that don't yet crash** — extra
  `Z_DELREF` on a zval that still has other holders. Latent
  bug, silent under sanitizers.
- **Persistent vs request-scoped confusion** — `pemalloc(...,1)`
  values being refcounted as if request-scoped.
- **Engine assertions** — the thousands of `ZEND_ASSERT`
  macros throughout the engine that fire on protocol
  violations.

**Setup choice:** `shivammathur/setup-php@v2` does not expose a
debug variant, so the job builds PHP from source. Build deps
are minimal (`libxml2-dev libsqlite3-dev pkg-config bison
re2c`) — XML/DOM/sqlite3 are PHP 8 default-on extensions our
tests touch transitively (json built-in, FFI skips when not
loaded). `actions/cache@v4` keys on PHP version + a manual
`-vN` salt, so cache invalidation is explicit. Cold-cache
build is ~5–8 min; warm-cache hit is seconds.

**Why `-O0 -g` for the extension build:** debug PHP is slow
anyway, and `-O0` keeps line numbers in any abort traces
useful. Otherwise the canonical 19a flag set
(`-Wall -Wextra -Werror -Wshadow -Wstrict-prototypes
-Wmissing-prototypes`) applies.

**Local dev unaffected.** No debug PHP required on the dev
machine; CI is the gate. The local `scripts/sanitize.sh` /
`scripts/memcheck.sh` cover overflow/UAF/UB/memory-leak shapes
that are far more common in this codebase. Refcount-protocol
violations are rare enough that catching them on every CI
run, not on every local rebuild, is the right cost ratio.

**How to apply:** any PR that touches `PHP_METHOD` return
paths, zval construction (`object_init_ex`, `ZVAL_ARR`,
`RETURN_*`), refcount manipulation (`Z_ADDREF`, `Z_DELREF`,
`zval_ptr_dtor`), or the `do_operation` dispatch must
pass the `debug-php` job. EXPECTF widening for debug-build
shutdown stderr is allowed (one-line comment per phpt
explaining the divergence); fixing the underlying refcount
bug is preferred where scoped.

### 2026-05-04 — Sprint: Bool surface follow-up — bitwise + logical (Story 20, 2 of 2)

#### 39. Bitwise ops reject float input

`NDArray::bitwiseAnd` / `bitwiseOr` / `bitwiseXor` / `bitwiseNot`
accept only `bool` / `int32` / `int64` input. Float input throws
`\DTypeException` with message
`"<op>: float dtypes not supported (got <dtype>); cast to int first"`.

**Why:** matches NumPy (`np.bitwise_and(np.float64(...), ...)`
raises `TypeError`). Alternative would be implicit cast to int —
too surprising for a numerical library; users who want bit-level
semantics on a float array should opt in by casting explicitly,
not be silently rounded.

**Output dtype:** promotion of inputs via the existing
`numphp_promote_dtype` (5×5 table from Story 17). Slice {bool,
int32, int64} produces:

- `bool ⊕ bool` → bool
- `bool ⊕ int32` → int32
- `int32 ⊕ int64` → int64
- `int64 ⊕ int64` → int64

**`bitwiseNot` on bool is logical NOT, not C-level `~`** —
`bitwiseNot(true) === false`, `bitwiseNot(false) === true`.
Matches NumPy (`~np.bool_(True)` returns `False`). The kernel
special-cases bool in `numphp_bitwise_not`. Locked by
`tests/068-bitwise.phpt`.

**How to apply:** keep the float-rejection at the dispatch
boundary (top of `numphp_bitwise` / `numphp_bitwise_not`), so the
error fires before any allocation or iterator setup. New unary or
binary bitwise variants (e.g. shift ops if added later) must use
the same `bitwise_reject_float` helper for consistent error
messages.

#### 40. Logical ops always output bool, regardless of input dtype

`NDArray::logicalAnd` / `logicalOr` / `logicalXor` / `logicalNot`
accept any numeric or bool input — values are coerced to bool
element-wise (any non-zero → true; NaN → true, matching NumPy and
PHP `(bool)NAN`). **Output dtype is always `NUMPHP_BOOL`,**
regardless of input dtypes.

**Why:** matches NumPy (`np.logical_*` always returns a bool
array) and is consistent with the comparison ops shipped in
Story 17 (which also always output bool, per decision 35
lineage). The whole point of "logical" ops is to cross the
truthy-ness threshold and return a clean bool answer.

**`logicalXor` does not short-circuit** — XOR by definition needs
to see both operands. `logicalAnd` / `logicalOr` short-circuit
element-wise where applicable. Documented in `docs/api/ndarray.md`.

**How to apply:** the kernel allocates the output as
`NUMPHP_BOOL` up front (`numphp_logical` and `numphp_logical_not`
in `src/ops.c`), independent of input dtype. The truthy-coercion
helper `read_truthy` handles the float-NaN case explicitly so the
behaviour stays the same regardless of which of the four input
dtype combinations is being walked.

## Open Items / Caveats

- ~~**LAPACK symbol portability on macOS.**~~ **Fixed in Story 10.** `config.m4` now probes both `dgetri_` (Linux convention) and `dgetri` (macOS Accelerate). `lapack_names.h` aliases symbols when the no-underscore variant is in use. macOS CI lane is now blocking.
- **gcov coverage gate parked, filter widened in Story 13 Phase B.** Job stays `continue-on-error: true` for now — the gate is informational, not blocking. Filter widened from `ndarray.c + ops.c` only to all 7 C source files in `config.m4`, so the reported number reflects the real surface. Measured 86% lines at the time of widening. Flipping to blocking is deferred to a later sprint when v0.1.0 release-quality scrutiny is on the table.
- **Nothing depends on `nditer.c` yet.** The empty TU is listed in `PHP_NEW_EXTENSION` so the build graph stays stable. If a header declaration is added without a definition the linker fails loud — that is the intended canary.
- **Transpose-trick zero-copy deferral.** `inv` and `det` could skip the F-contig copy via the column-major reinterpretation trick (row-major bytes interpreted as column-major = transpose; `inv(A^T) = inv(A)^T` and `det(A^T) = det(A)`). `solve`, `svd`, `eig` cannot use the trick safely. v1 ships uniform copy for simplicity; revisit if profiling shows the copy is hot.
- ~~**`do_operation` compound-assign refcount leak.**~~ **Fixed in sprint 19b-fix (decision 37 amendment).** `$c += $b` was leaking 48 bytes per call — PHP's `ZEND_ASSIGN_OP` opcode dispatches as `add_function(&$c, &$c, &$b)`, so `result == op1` in pointer terms; our handler overwrote `result` via `object_init_ex` without releasing prior content. Four prior fix attempts (within sprint 19b and 19b-fix) failed because they used `IS_OBJECT(result)` as the discriminator — wrong signal, since PHP reuses VM tmp slots without clearing type-info. Investigation surfaced the GMP idiom (`ext/gmp/gmp.c:480-496`): pointer-equality discriminator `result == op1`, snapshot+redirect, dtor the snapshot after success. 6-line outer wrapper around the existing `do_binary_op_core`, no inner-kernel changes. Locked by sanitizer CI now running with `detect_leaks=1` + `lsan.supp`.
- **Story 11 Phase C (Arrow IPC) deferred post-1.0.** Phases A + B shipped. Phase C blocked on an unresolved install-ergonomics tradeoff: libarrow C++ is not in Ubuntu 24.04 universe, and Apache's APT repo install path failed mid-setup (doubled `ubuntu` in URI). The cleaner alternative is vendoring nanoarrow (single-file C, permissive license, IPC-scoped), but no user has yet asked for Arrow interop — speculative work. Revisit after Stories 12 (PECL) and 13 (docs) ship; by then real interop demand may have surfaced and the upstream landscape (Ubuntu inheritance, nanoarrow maturity) will have moved. User story stays in `docs/user-stories/backlog/11-interoperability.md`.

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
- **2026-05-01 — `documentation-pass` (Story 13 Phase A)** delivered the release-quality documentation pass: complete API reference at `docs/api/` covering all 65 PHP-visible methods on `NDArray` / `Linalg` / `BufferView` plus the four exception classes (each entry: signature, params table, returns, throws-list, runnable example); end-to-end getting-started guide; five concept guides (dtype promotion, broadcasting, views-vs-copies, NaN policy, round-half divergence); a NumPy↔NumPHP cheatsheet (~50 rows with divergences flagged); and the repo-root `README.md`. Format choice locked as decision 25 (plain Markdown with strict per-method template — DocBook rejected as toolchain overkill); naming convention locked as decision 26 (`numphp` lowercase in code/CLI, `NumPHP` mixed-case in prose). Snippet verification harness (`/tmp/verify_docs.php`) ran 48 fenced code blocks against the live extension and surfaced an undocumented divergence: `Countable::count(NDArray)` returns total element count (matches `size()`), not leading-axis size as NumPy's `len()` does. Locked since Story 4 by `tests/013-countable.phpt` but never written down — now decision 27 and prominently documented in `docs/api/ndarray.md`, `docs/cheatsheet-numpy.md`, and the getting-started "common gotchas" section. Build clean at -Wall -Wextra; 53 phpt + 1 skipped (FFI). No code changes to source files — pure documentation sprint. **Phase A only — story 13 stays in backlog with Phase A annotation; Phase B (examples + test-coverage audit) and Phase C (benchmarks) ship in follow-up sprints. Story 14 (community + outreach) was split out from the original Story 13 prior to this sprint per user request.** Deferred to Phase B: snippet-as-test enforcement (CI-extracted code-block runner), `examples/` directory of runnable scripts, gcov gate flip.
- **2026-05-01 — `13b-examples-and-tests` (Story 13 Phase B)** delivered the "does the API survive realistic use" pass. Five runnable example scripts in `examples/` (linear regression, k-means, image-as-array, time-series, CSV pipeline), each with a checked-in `.expected` and a new CI job that diffs them. Snippet harness (`tests/100-doc-snippets.phpt` + `tests/_helpers/snippet_runner.php`) extracts every fenced ```php block from user-facing docs and runs it on every PR — 75 doc snippets are now regression-tested automatically. 6 gap-closure phpt tests (055–060) following an audit captured in `docs/coverage-audit-2026-05-01.md`: DType throw paths, IndexException axis-OOR across squeeze/expandDims/concatenate/stack, BLAS k=1 edges, decision 9 locked at value-overflow scale, ±inf through every reduction, and the segfault regression test below. **Real bug found and fixed:** `NDArray::fromArray([[1, [99]]])` (mixed scalar/array siblings at the same depth) crashed PHP — rank-inference walk in `fromarray_walk` allowed siblings to extend ndim past the leaf depth a previous scalar had locked. Fixed via `rank_locked` flag; regression test `060-fromarray-mixed-depth.phpt`. Doc tidy: `full()` and `fromArray()` throws-lists corrected — the docs claimed `\DTypeException` for non-numeric values and NaN-cast-to-int, neither true (silent PHP cast, matching NumPy on NaN→int). gcov filter widened from `ndarray.c + ops.c` only to all 7 C sources in `config.m4`; gate stays `continue-on-error: true` — flipping is deferred to a release-quality sprint, not pre-release iteration. `scripts/coverage.sh` added as a single canonical local entry point that contains its build artefacts (cleans `a-conftest.gc*` left by autoconf probes; reports go to `coverage/`, gitignored). 60 phpt run + 1 skipped (FFI), build clean at -Wall -Wextra. Version bumped to **0.0.11**. **Phase B only — story 13 stays in backlog with Phases A+B annotated as shipped; Phase C (benchmarks) remains, deferred to a release-quality sprint.** Deferred from this sprint: gcov gate flip to blocking, CI artifact upload, ratifying the coverage threshold or snippet-test convention as architectural decisions (they're tooling, not architecture). Sprint scope was re-narrowed mid-execution after drift toward CI polish — keep Phase B about exercising the API and protecting the docs, leave release-engineering to release time.
- **2026-05-01 — `15-project-layout` (Story 15)** mechanical layout sprint: 15 tracked C/H files moved from project root to `src/` via `git mv` so `git blame` follows. `config.m4` updated to reference `src/foo.c` paths and `PHP_ADD_INCLUDE([$ext_srcdir/src])` added so cross-file `#include "numphp.h"` keeps resolving. `LICENSE` (BSD 3-Clause) added at root — was previously a "TBD" placeholder in README. README's license section updated to point at the new file. Decisions 28 (sources under `src/`) and 29 (BSD 3-Clause) locked. No behavioural change: 60 phpt + 1 FFI-skip pass, all 5 examples still match `.expected`, doc snippet harness still green, build clean at `-Wall -Wextra`. Version bumped to **0.0.12**. Story 15 → `done/`. Net effect on root: ~20 tracked files dropped to ~6, all of them meta files a newcomer reads first.
- **2026-05-01 — `13c-benchmarks` (Story 13 Phase C)** delivered the first reproducible numphp-vs-NumPy comparison. New `bench/` directory with mirrored runners (`run.php` + `run.py`) driven from a shared `scenarios.json`, plus `compare.py` for the Markdown table, `fingerprint.sh` for hardware/BLAS/version metadata, and `run.sh` orchestrating the lot. Methodology locked as decision 30: 7 timed runs, drop slowest, median + min + max; deterministic seed; per-scenario fixture allocation excluded except where the fixture is the subject. NumPy lives in a project-local `bench/.venv/` (gitignored) so the system Python isn't touched (PEP 668-compliant). 11 scenarios cover element-wise add/multiply, matmul (f64 + f32), `sum` along each axis, `fromArray` / `toArray` round-trip, `Linalg::solve`, `Linalg::inv`, and a sub-microsecond `slice` view-creation timer. First run committed as `docs/benchmarks.md` with the maintainer's hardware fingerprint. Headline: matmul + linalg at parity (~1.0×), interop *faster* than NumPy on numphp (`fromArray`/`toArray` ~0.35×), element-wise ~2.5× slower, axis-0 sum 15× slower (cache-unfriendly direction, no vectorised per-axis kernel yet). New smoke test `tests/061-bench-runner-smoke.phpt` invokes the runner via `proc_open` and asserts exit 0 + one JSON record per scenario — catches "runner is broken" without locking flaky timing numbers. Story 13 is now fully shipped (Phases A + B + C); story file moved to `done/`. Version bumped to **0.0.13**. **Out of scope, deliberately:** external publication (Story 14), CI integration (shared runners are too noisy), comparison against other PHP numeric libraries.
- **2026-05-01 — `18-bool-and-comparisons` (Story 17)** added the `bool` dtype, six static comparison methods, and `NDArray::where`. **Decisions 32–35** locked. (1) `bool` is a 1-byte canonical-0/1 dtype with lenient reads and write-canonicalisation — bool ⊕ bool arithmetic falls through to the slow path and behaves as logical OR/AND/XOR (matches NumPy). The promotion table widened to 5×5; `bool` sits at the bottom (`bool ⊕ X = X`). (2) Six comparison ops (`eq`, `ne`, `lt`, `le`, `gt`, `ge`) as `public static` methods returning bool NDArrays of the broadcast shape. NaN policy corrected from the story's literal wording to IEEE 754 / NumPy: `eq`/`lt`/`le`/`gt`/`ge` return false on NaN, **`ne` returns true on NaN**. (3) `NDArray::where(cond, x, y)` — cond must be bool (else `\DTypeException`); output dtype is the promotion of `x` and `y` (cond's dtype is irrelevant); broadcasts across all three operands using the existing 4-operand nditer (`NUMPHP_ITER_MAX_OPERANDS = 4` already accommodated). 4 new phpt tests (064–067) cover dtype plumbing × factories × fromArray/toArray × save/load × promotion × every comparison op × broadcasting × NaN policy × where with all-array / scalar-mixed / 3-way-broadcast / cond-not-bool / shape-mismatch × bool through every existing reduction × bool through transpose/reshape/concatenate. The sprint-16 element-wise fast path required one extra predicate (`out_dt != NUMPHP_BOOL`) so bool arithmetic always takes the slow path where write-canonicalisation lives. Build clean at `-Wall -Wextra`; 67/67 + 1 skipped. Docs: `docs/api/ndarray.md` (Comparisons + Where sections), `docs/concepts/dtypes.md` (5×5 promotion table + bool reductions row + decisions 32/34 cross-references), `docs/concepts/nan-policy.md` (Comparisons under NaN section), `docs/cheatsheet-numpy.md` (Comparisons & where rows). Version bumped to **0.0.18**. **Out of scope, deliberately:** boolean indexing (depends on this sprint, follow-up), `any`/`all` reductions (small follow-up), bitwise ops on bool, comparison-operator overloading (decision 35).
- **2026-05-01 — `17-cumsum-cumprod` (Story 18)** added `cumsum` / `cumprod` (and `nancumsum` / `nancumprod`) as instance methods on `NDArray`. Single new kernel `numphp_cumulative` in `ops.c` patterned on `numphp_reduce`; `do_cumulative_method` in `ndarray.c` patterned on `do_reduce_method`. `?int $axis = null` flattens (1-D output of length `size()`); integer axis cumulates along that axis preserving input shape; negative axis allowed; out-of-range throws `\ShapeException`. Output dtype: int → `int64`, f32 → `f32`, f64 → `f64`. NaN propagates by default; `nancumsum` / `nancumprod` skip NaN by treating it as the additive (0) / multiplicative (1) identity (all-NaN slice → all 0 / all 1, no exception, symmetric with how `nansum` already behaves). Integer dtypes alias the plain forms in the nan-variants since they cannot hold NaN. **Decision 31** locks the deliberate divergence from NumPy: `cumprod` on int promotes to `int64` (NumPy preserves input dtype) — same silent-overflow concern that drove decision 9 for `sum`, magnified for products which grow faster. 2 new phpt tests (062-cumulative, 063-cumulative-nan) cover all four numeric dtypes × axis=null/integer/negative × NaN propagation × all-NaN slice × 3-D strided walk × dtype-promotion table × empty input. Build clean at `-Wall -Wextra`; 63/63 + 1 skipped. Docs: `docs/api/ndarray.md` (cumulative-reductions section + nan-variants subsection), `docs/concepts/dtypes.md` (reductions table + footnote), `docs/cheatsheet-numpy.md` (5 new rows including the `cumprod` divergence flag). Version bumped to **0.0.17**. **Out of scope, deliberately:** multi-axis cumulation (NumPy doesn't either), Kahan/Neumaier compensated summation (default paths everywhere in NumPHP use simple fold; revisit on accuracy bug), in-place variants (consistent with the broader "no in-place ops yet" stance), `bool` input handling (deferred to Story 17 in a follow-up sprint).
- **2026-05-03 — clean-rule-no-recursive-rm (small fix)** Replaced phpize's recursive `find . -name '*.so' | xargs rm -f` (and friends for `.lo`, `.o`, `.dep`, `.la`, `.a`, `.libs/`) with explicit path-scoped rules. New `Makefile.frag` at project root contains the override; new `PHP_ADD_MAKEFILE_FRAGMENT` call in `config.m4` hooks it. Survives `phpize --clean`. **Why:** the upstream recursive rules silently delete any matching file under the project tree — surfaced when a sprint cleanup cycle wiped numpy's compiled `.so` files inside `bench/.venv/`. Recursive rm in a build tool is a foot-gun; structural fix is to stop using it. **Make warns** "overriding recipe for target 'clean'" — expected and intended; it's the override doing its job. Version bumped to **0.0.22**. 67/67 phpt + 1 FFI skip pass; full clean/rebuild cycle verified end-to-end with the bench round-trip.
- **2026-05-03 — `19b-fix-do-operation-leak` (Story 19, interstitial fix)** closed the compound-assign refcount leak ASan flagged in 19b. Investigation pinned the discriminator: PHP's `ZEND_ASSIGN_OP` opcode dispatches as `add_function(var_ptr, var_ptr, value)` (`Zend/zend_execute.c:1652`), making `result == op1` a pointer-exact signal — the heuristics our four prior fix attempts used (`Z_TYPE_P(result) == IS_OBJECT`) were wrong because PHP reuses VM tmp slots without clearing type-info. Fix lifted verbatim from `ext/gmp/gmp.c:480` (`gmp_do_operation`): if `result == op1`, snapshot op1 to a local, redirect op1 to the snapshot, run the inner kernel, dtor the snapshot on success. 6-line outer wrapper around `numphp_do_operation`; existing `do_binary_op_core` inner kernel untouched. New `lsan.supp` at repo root suppresses three categories of PHP-internal startup leaks (`zend_startup_module_ex`, `_dl_init`, `getaddrinfo` — each annotated with rationale; rejects any numphp_* suppression). CI `sanitizers` job + `scripts/sanitize.sh` flipped `detect_leaks=0` → `detect_leaks=1` with `LSAN_OPTIONS=suppressions=lsan.supp`. Decision 37 amended (leak detection now ON; deferred-fix paragraph dropped). Open-item entry retired. 67/67 phpt + 1 FFI skip pass under both regular AND sanitizer-with-leaks-on builds. Version bumped to **0.0.21**.
- **2026-05-03 — `19b-asan-ubsan` (Story 19, Phase B)** wired ASan + UBSan into CI. New `sanitizers` CI job runs the phpt suite under `-fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g` with gcc (clang dropped — no advantage at our scale, saves an apt install step). `LD_PRELOAD` for libasan + libubsan ensures ASan loads before libc malloc (extension is `dlopen`'d into PHP after startup). `USE_ZEND_ALLOC=0` makes PHP's pool allocator transparent to ASan. **One real bug surfaced** by the sanitizer run: `numphp_zval_wrap_ndarray` leaks the prior `$c` object on `$c += $b` compound assignment — PHP passes `result == op1` to `do_operation`, so `object_init_ex(result, ...)` overwrites without releasing. Initial fix attempt regressed 9 tests with a segfault; per cap-iteration rule the bug is deferred (open item recorded; fix is to dtor only in `do_binary_op_core` rather than in the shared wrap function). **`detect_leaks=0`** while the bug is open: PHP-engine startup leaks (`getaddrinfo`, `dlopen` `_init`, module table) plus our one extension leak make LSan unusable; ASan and UBSan still catch overflows, UAF, signed overflow, alignment, shifts. Local convenience scripts: `scripts/sanitize.sh` (mirrors CI) and `scripts/memcheck.sh` (valgrind via `run-tests.php -m`; fails fast with apt-install hint if valgrind isn't installed — we don't auto-install per user policy). **Decision 37** locked. 67/67 phpt + 1 FFI skip pass under sanitizers; baseline build also still green at the 19a flag set. Version bumped to **0.0.20**. **Out of scope, deliberately:** debug-PHP / `ZEND_RC_DEBUG` (sprint 19c); fixing the `do_operation` leak (follow-up sprint, requires a careful refcount audit); skipping the deliberate use-after-free CI-sanity-check branch (the working sanitizer build verified end-to-end via the real do_operation finding — that IS the sanity check, just unintentional).
- **2026-05-03 — `19a-compiler-flag-hardening` (Story 19, Phase A)** tightened the static check surface. New canonical CFLAGS: `-Wall -Wextra -Werror -Wshadow -Wstrict-prototypes -Wmissing-prototypes`. Decision 36 locked. Two categories of fix needed: (1) one pre-existing `-Wunused-parameter` hit on `BufferView::__construct` (the only no-args method that didn't call `ZEND_PARSE_PARAMETERS_NONE();` — fixed by adding it, matching the pattern of every other no-args method in the codebase); (2) 87 `-Wmissing-prototypes` warnings on Zend-macro-generated symbols (`zim_NDArray_*`, `zim_Linalg_*`, `zim_BufferView_*` from `PHP_METHOD`; `zm_startup_numphp` / `zm_info_numphp` / `get_module` from `PHP_MINIT_FUNCTION` / `PHP_MINFO_FUNCTION` / `ZEND_GET_MODULE`). Fixed by file-scope `#pragma GCC diagnostic ignored "-Wmissing-prototypes"` push/pop blocks in the four source files containing macro-generated functions (`numphp.c`, `ndarray.c`, `linalg.c`, `bufferview.c`); rationale is that these functions are referenced only via `zend_function_entry` tables by function pointer — they can't be `static` (table takes their address) and don't belong in a header (TU-internal). All 5 CFLAGS sites in `.github/workflows/ci.yml` updated (build-test, examples, valgrind, coverage, macos); coverage job exempted from `-Werror` because `--coverage` instrumentation can introduce diagnostics that don't reflect source defects (job stays `continue-on-error: true` regardless). Local sanity check: deliberately shadowed an `int ndim_out` in `src/ops.c:443`, confirmed `-Werror=shadow` failed the build, reverted; same shape will fail the CI build-test job once pushed. Build clean at the new flag set; 67/67 phpt + 1 FFI skip pass; doc-snippet harness green. Version bumped to **0.0.19**. **Deferred to a follow-up sprint:** `-Wcast-align`, `-Wnull-dereference`, `-Wdouble-promotion` — each likely needs a project-wide source-pattern decision (alignment policy / null-check policy / f32 literal suffix policy).
- **2026-05-01 — `16-fastpath-optimizations` (Story 16)** closed the two clear-cause weak spots from the 0.0.13 benchmark. Two kernel additions, no API change, no SIMD intrinsics. (1) Element-wise contiguous fast path in `do_binary_op_core`: when both inputs and output are C-contiguous, same dtype, identical shape (no broadcasting), skip the iterator + per-element function-call dispatch and emit a flat typed-pointer loop that the compiler auto-vectorises at `-O2`. Mixed-dtype, broadcasting, and non-contig sources still take the original slow path. (2) Axis-0 sum tiled kernel in `numphp_reduce`: 2-D C-contiguous f32/f64 sources, axis=0, no NaN-skip dispatch to a column-strip kernel that processes 32 columns in lockstep with pairwise recursion preserved per column, so the leaf reads contiguous cache lines instead of one cache miss per row. Output is bit-identical to the slow path because each column's recursion structure is unchanged. Numbers (vs NumPy): elementwise add 2.64× → **1.01×**, mul 2.51× → **1.05×**, sum axis=0 15.48× → **4.40×**. All other scenarios stable within noise (matmul 0.96×, linalg parity, interop ~0.35× — *faster* than NumPy still). All 61 phpt tests still pass with bit-identical output verified by `032-`, `038-`, `058-`. `docs/benchmarks.md` refreshed; `bench/run.sh` reproduces. Version bumped to **0.0.14**. **Out of scope, deliberately:** SIMD intrinsics, axis-1 fast path, fast paths for other reductions, new dtypes, new functions. The remaining ~4× gap on axis reductions is no longer cache-bound — closing it would require intrinsics, deferred.
- **2026-05-04 — `20a-bool-reductions` (Story 20, 1 of 2)** added six reductions finishing the bool surface from the reduction side: `any` / `all` (short-circuiting OR/AND, output always bool), `prod` / `nanprod` (multiplicative, with int → int64 promotion), `countNonzero` (output always int64), and `ptp` (peak-to-peak, preserves input dtype including the `true - false === true` bool case). Pure expansion of the existing `numphp_reduce` machinery — extended `enum numphp_reduce_op` in `src/ops.h` with 5 entries (PROD shares `nanprod` via the existing `skip_nan` flag, same idiom as `nansum`), updated `reduce_out_dtype`, added 5 new cases to the `reduce_line` switch dispatch, and wired empty-input identities into `numphp_reduce` (any → false, all → true, prod → 1, countNonzero → 0, ptp → throws `\NDArrayException`). Six thin PHP_METHOD wrappers + method-table entries in `src/ndarray.c`; signatures uniform with existing reductions (`?int $axis = null, bool $keepdims = false`). NaN policy follows existing precedent: `prod` propagates NaN by default; `nanprod` treats NaN as multiplicative identity 1 (all-NaN slice → 1, symmetric with `nansum` → 0 and `nancumprod` → all-1); `any`/`all` count NaN as truthy (matches NumPy / `(bool)NAN`); `countNonzero` counts NaN as non-zero. **Decision 31 amended (no new decision number)** — the sprint plan reserved decision 39 for `prod` int → int64, but the rationale collapsed to a one-line consequence of the existing `cumprod` decision (same silent-overflow concern, same NumPy divergence flag), so per the sprint plan's "fold if it collapses" rule the rule was added as a column to decision 31's table and a sub-bullet at the end of that decision's body rather than allocated a new number. 2 new phpt tests (`064-any-all`, `065-prod-nanprod-countnonzero-ptp`) cover all four numeric dtypes × bool input × axis null/integer/negative × keepdims × NaN propagation × all-NaN slice × empty input × dtype assertions × overflow-scale assertion (`[100000, 100000, 100000]` of `int32` returns `1_000_000_000_000_000`, fits int64 but wraps int32). Build clean at the canonical CFLAGS; 69/69 phpt + 1 FFI skip pass; doc-snippet harness green (now ~85 fenced blocks with the 6 new method examples). Local `scripts/sanitize.sh` surfaces only pre-existing subprocess leaks in `/usr/bin/sed` and `/usr/bin/make` (LD_PRELOAD inheriting into child processes — verified pre-existing by stash + re-run on master); no numphp/zim_ frames in any leak trace. Version bumped to **0.0.24**. **Story 20 stays in `backlog/`** — sprint 20b (bitwise + logical: `bitwiseAnd/Or/Xor/Not` + `logicalAnd/Or/Xor/Not`, decisions 39 + 40) closes it. **Out of scope, deliberately:** boolean mask indexing (Story 21, the architectural lift), shift ops, isclose/allclose/isnan/isinf/isfinite, `do_operation` overloads for bitwise/logical, in-place variants, performance optimisation (no SIMD or fast paths beyond what reduction infrastructure already provides).
- **2026-05-04 — `20b-bool-bitwise-logical` (Story 20, 2 of 2)** added eight elementwise methods closing Story 20. Bitwise: `bitwiseAnd` / `bitwiseOr` / `bitwiseXor` / `bitwiseNot` — bool / int input only; float input throws `\DTypeException` at the dispatch boundary via the new `bitwise_reject_float` helper (decision 39); output dtype is the promotion of inputs within {bool, int32, int64} via the existing `numphp_promote_dtype`. **`bitwiseNot` on bool is logical NOT** (`bitwiseNot(true) === false`), not C-level `~`; the kernel special-cases bool — using C-level `~` on a {0, 1} canonical byte would produce -1/-2 which `numphp_write_scalar_at` would canonicalise back to 1, so both naive paths give wrong answers. Locked by an explicit assertion in `tests/068-bitwise.phpt`. Logical: `logicalAnd` / `logicalOr` / `logicalXor` / `logicalNot` — accepts any numeric/bool input, coerces element-wise to bool via the new `read_truthy` helper (any non-zero → true; NaN → true, matching NumPy / `(bool)NAN`); output is always `NUMPHP_BOOL` regardless of input dtypes (decision 40). `logicalXor` walks the full inputs (no short-circuit; XOR can't). Kernels in `src/ops.c` patterned on `numphp_compare` (broadcast shapes, allocate output, walk via `numphp_nditer`); two new dispatch enums in `src/ops.h` (`numphp_bitwise_op` 3 entries + `numphp_logical_op` 3 entries — unary `bitwiseNot` and `logicalNot` are separate functions, not a "NOT" enum value, since unary nditer is a separate code path). Eight thin PHP_METHOD wrappers in `src/ndarray.c` via `do_bitwise_method` and `do_logical_method` dispatchers patterned on `do_compare_method`; new `arginfo_binop_bool` and `arginfo_unop_bool` shared across the eight methods; method-only API per decision 35 lineage (no `do_operation` overload — risk/reward unattractive right after the sprint 19b-fix compound-assign work). 2 new phpt tests (`068-bitwise`, `069-logical`) cover bool ⊕ bool, int ⊕ int, bool ⊕ int promotion, scalar-on-either-side, broadcasting (1-D vs 2-D), `bitwiseNot` on bool + int, **f32/f64 input throws `\DTypeException` with expected message** (one assertion per binop op + one for unary), empty input, shape mismatch, NaN coercion in logical ops. Build clean at the canonical CFLAGS; 71/71 phpt + 1 FFI skip pass; doc-snippet harness green (now ~95 fenced blocks). One discovery during execution: PHP scalar `true` / `false` go through `scalar_to_0d_ndarray` and wrap as int64 0-D (pre-existing convention since Story 17 / decision 32; not changed here to avoid scope creep into a shared helper) — so `bitwiseAnd(true, $bool_array)` produces int output, not bool. Documented in test comment; left as-is. Version bumped to **0.0.25**. **Story 20 closed** (file moved to `done/`). **Out of scope, deliberately:** shift ops (`<<`, `>>` / `bitwiseLeftShift` / `bitwiseRightShift` — trivial but deferred until demand surfaces), `isclose` / `allclose` / `isnan` / `isinf` / `isfinite` (different cluster), `do_operation` overload for `& | ^ ~` (decision 35 lineage; revisit if demand surfaces), in-place variants, fixing `scalar_to_0d_ndarray` to recognise `IS_TRUE` / `IS_FALSE` as bool (cleaner long-term but touches a shared helper used by every binop dispatch — separate sprint).
