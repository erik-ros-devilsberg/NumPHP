# Resume Notes — 2026-05-04 (after sprint 20b — Story 20 closed)

## Where we are

**Version 0.0.25.** Pre-release, iterative. Building toward 0.1.0.

**23 sprints + 1 fix shipped.** Build green at
`-Wall -Wextra -Werror -Wshadow -Wstrict-prototypes -Wmissing-prototypes`
(sprint 19a); ASan + UBSan + LSan run on every CI build with
`detect_leaks=1` (sprint 19b + 19b-fix); debug-PHP CI lane runs
phpt + examples with `ZEND_RC_DEBUG=1` + `ZEND_ALLOC_DEBUG=1` +
`USE_ZEND_ALLOC=0` (sprint 19c); 71/71 phpt + 1 FFI skip + doc
snippet harness running ~95+ fenced ```php blocks. **Story 20
(bool surface follow-up) closed.**

| # | Sprint | Stories | Version |
|---|--------|---------|---------|
| 1-13 | (see CHANGELOG) | | 0.0.1 → 0.0.12 |
| 14 | 13c-benchmarks (Story 13 Phase C) | 13C | 0.0.13 |
| 15 | 16-fastpath-optimizations (Story 16) | 16 | 0.0.14 |
| 16 | (skipped — version numbering rule changed mid-flight) | — | — |
| 17 | 17-cumsum-cumprod (Story 18) | 18 | 0.0.17 |
| 18 | 18-bool-and-comparisons (Story 17) | 17 | 0.0.18 |
| 19 | 19a-compiler-flag-hardening (Story 19, Phase A) | 19a | 0.0.19 |
| 20 | 19b-asan-ubsan (Story 19, Phase B) | 19b | 0.0.20 |
| 21 | 19b-fix-do-operation-leak | — | 0.0.21 |
| —  | clean-rule-no-recursive-rm | — | 0.0.22 |
| 22 | 19c-debug-php (Story 19, Phase C) | 19c | 0.0.23 |
| 23 | 20a-bool-reductions (Story 20, 1 of 2) | 20a | 0.0.24 |
| 24 | 20b-bool-bitwise-logical (Story 20, 2 of 2) | 20b | 0.0.25 |

**Backlog:** Story 11 Phase C (Arrow IPC) post-1.0, Story 12 (PECL
packaging — parked), Story 14 (community + outreach). Story 19,
Story 20 done. Next pickup per the parity roadmap is the CI-fix
sprint (triage anything 19c's debug-PHP / 20a / 20b surfaces on
push), then Story 21 (boolean mask indexing — the architectural
lift that generalises `offsetGet` / `offsetSet`).

## What just landed (sprint 20b)

Eight new elementwise methods closing Story 20: `bitwiseAnd` /
`bitwiseOr` / `bitwiseXor` / `bitwiseNot` (4 bitwise; bool + int
input only, float throws `\DTypeException`) and `logicalAnd` /
`logicalOr` / `logicalXor` / `logicalNot` (4 logical; any input,
output always bool, NaN coerces to true).

- **Bitwise** patterned on `numphp_compare` — broadcast shapes,
  allocate output as promoted dtype, walk via `numphp_nditer`.
  Float rejection at the dispatch boundary (`bitwise_reject_float`
  helper in `ops.c`). `bitwiseNot` on bool is logical NOT
  (`~true === false`); on int it's C-level `~`.
- **Logical** mirrors bitwise but with element-wise truthy
  coercion (`read_truthy` helper) and bool output regardless of
  input dtype. `logicalXor` walks the full inputs (no
  short-circuit; XOR can't).
- Method-only API per decision 35 lineage. No `do_operation`
  expansion in this sprint — risk/reward unattractive right after
  the sprint 19b-fix compound-assign work.
- **Decisions 39 + 40** locked: bitwise rejects float; logical
  always outputs bool.

2 new phpt tests (`068-bitwise`, `069-logical`); 69 → 71 phpt + 1
FFI skip pass. Build clean at the canonical CFLAGS. Story 20
file ready to move from `backlog/` → `done/` on wrap.

## What landed in sprint 20a

Six new reductions finishing the bool surface from the reduction
side: `any` / `all` / `prod` / `nanprod` / `countNonzero` / `ptp`.
Story 20 1-of-2; bitwise + logical clusters were the 20b sprint.

- **`any` / `all`** — short-circuiting OR / AND. Output dtype always
  bool. Non-bool input coerces element-wise (any non-zero → true;
  NaN → true, matching NumPy / `(bool)NAN`). Empty → `any` is
  false, `all` is true (vacuous truth).
- **`prod` / `nanprod`** — multiplicative reduction. Decision 31
  amended (no new decision number) to extend the existing
  `cumprod` int → int64 promotion rule to `prod` and `nanprod`.
  Same silent-overflow rationale; same NumPy divergence flag in
  the cheatsheet. Locked by an explicit overflow-scale assertion
  in `065-...phpt` (`[100000, 100000, 100000]` of int32 →
  `1_000_000_000_000_000`, fits int64 but wraps int32).
- **`countNonzero`** — output dtype always int64. NaN counts as
  non-zero (matches NumPy: `bool(NAN) === true`).
- **`ptp`** — `max - min`, preserves input dtype. Bool case:
  `[true, false]->ptp() === true` (matches NumPy). Empty input
  throws `\NDArrayException`.

All six reuse the existing `numphp_reduce` machinery — extended
the enum, added 5 new cases to the `reduce_line` dispatch (PROD
shares with nanprod via `skip_nan`), updated `reduce_out_dtype`,
and added 6 thin PHP_METHOD wrappers + arginfo entries. ~280
lines new C, well under the ~450-line estimate. 2 new phpt files
(`064-any-all`, `065-prod-nanprod-countnonzero-ptp`); 67 → 69
phpt + 1 FFI skip.

Local sanitize.sh on Ubuntu 24.04 surfaces pre-existing
subprocess leaks in `/usr/bin/sed` and `/usr/bin/make` from
LD_PRELOAD inheriting into child processes — none of the leak
traces touch numphp/zim_ frames. Verified pre-existing by stash +
re-run on master. CI sanitizers job is the authoritative gate.

## What landed in sprint 19c

Debug-PHP CI lane. Closes Story 19 (Build Quality Hardening).

- **New `debug-php` CI job** in `.github/workflows/ci.yml`. PHP
  built from source with `--enable-debug` (cached in
  `/opt/php-debug` keyed by version) — `shivammathur/setup-php`
  doesn't expose a debug variant. Build deps minimal:
  `libxml2-dev libsqlite3-dev pkg-config bison re2c`. Cold-cache
  build is ~5–8 min; warm-cache is seconds.
- **Test env:** `ZEND_RC_DEBUG=1`, `ZEND_ALLOC_DEBUG=1`,
  `USE_ZEND_ALLOC=0`. Runs phpt + examples-vs-`.expected` diff.
- **What this catches that ASan/UBSan/valgrind can't:** refcount
  leaks that aren't memory leaks (forgotten `zval_ptr_dtor`),
  refcount underflows that don't yet crash, persistent vs
  request-scoped confusion, and the engine's `ZEND_ASSERT`
  suite. Surface area in NumPHP that benefits: every
  `PHP_METHOD` returning a new NDArray (comparisons, `where`,
  reductions, shape ops, slice/concatenate), the class object
  handlers, `fromArray` / `toArray` traversal, and error paths
  that `RETURN_THROWS()` after partial setup.
- **Decision 38** locked: debug-PHP CI is the gate; local dev
  unaffected.

## What landed in sprint 19b-fix

Closed the do_operation compound-assign leak. LSan flipped on in
CI.

- **GMP idiom** (`ext/gmp/gmp.c:480`) lifted into `numphp_do_operation`:
  if `result == op1` (pointer-equal — confirmed via reading
  `Zend/zend_execute.c:1652` for `ZEND_ASSIGN_OP`), snapshot op1
  to a local, redirect, run the inner kernel, dtor the snapshot
  on success. 6-line outer wrapper; existing `do_binary_op_core`
  unchanged. Four prior fix attempts had used `IS_OBJECT(result)`
  as the discriminator and all failed — wrong signal because PHP
  reuses VM tmp slots without clearing type-info, so dtor'ing
  triggers UAF on stale-pointer slots.
- **`lsan.supp`** at repo root. Three suppression entries
  (`zend_startup_module_ex`, `_dl_init`, `getaddrinfo`) cover
  the PHP-internal startup leaks. Comments document each one;
  numphp_* symbols are rejected.
- **CI `sanitizers` job** + **`scripts/sanitize.sh`** flipped
  `detect_leaks=0` → `detect_leaks=1` with
  `LSAN_OPTIONS=suppressions=lsan.supp`. Any new leak in numphp
  code now fails the build.

## What landed in sprint 19b

ASan + UBSan in CI. New `sanitizers` CI job runs the phpt suite
under `-fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g`
with gcc. Decision 37 locked.

- **CI plumbing:** `LD_PRELOAD` for libasan + libubsan (extension
  is `dlopen`'d after PHP startup, so without preload ASan
  complains "runtime does not come first"). `USE_ZEND_ALLOC=0`
  so ASan sees every malloc. Compiler is gcc — clang dropped
  (no advantage at our scale, saves an apt install).
- **Real bug surfaced:** `numphp_zval_wrap_ndarray` leaks 48
  bytes on `$c += $b` — fixed in sprint 19b-fix above.
- **Local convenience:** `scripts/sanitize.sh` mirrors CI exactly;
  `scripts/memcheck.sh` runs the suite under valgrind (fails
  fast with apt install hint if valgrind isn't on `$PATH`).
  Recommended cadence — `sanitize.sh` during active work on
  refcounts/buffer arithmetic/error paths; `memcheck.sh` at
  sprint boundaries.

## What landed in sprint 19a

Compiler flag tightening. New canonical CFLAGS:
`-Wall -Wextra -Werror -Wshadow -Wstrict-prototypes -Wmissing-prototypes`.
Decision 36 locked.

- Two pre-existing issues fixed: `BufferView::__construct` was
  silently triggering `-Wunused-parameter` (fixed by adding
  `ZEND_PARSE_PARAMETERS_NONE();` to match every other no-args
  method); 87 `-Wmissing-prototypes` warnings on Zend-macro-
  generated function symbols suppressed via file-scope
  `#pragma GCC diagnostic` blocks in `numphp.c`, `ndarray.c`,
  `linalg.c`, `bufferview.c`.
- All 5 CFLAGS sites in `.github/workflows/ci.yml` updated.
  `coverage` job exempted from `-Werror` (instrumentation can
  introduce diagnostics that don't reflect source defects;
  job is informational).
- Local sanity check: deliberate shadow in `src/ops.c`
  failed the build at `-Werror=shadow`; reverted.

**Deferred to a future sprint:** `-Wcast-align`,
`-Wnull-dereference`, `-Wdouble-promotion` — each likely needs
a project-wide source-pattern decision before adoption.

## What landed in sprint 18

Bool dtype + six comparison ops + `where`. Three things, one sprint,
each strictly depending on the prior. Decisions 32–35 locked.

- **`bool` dtype** — `NUMPHP_BOOL=4`, 1 byte/element, canonical 0/1
  (writes canonicalise; reads accept any non-zero). Recognised in
  every factory, fromArray/toArray, save/load. Promotion table
  widened to 5×5; `bool` sits at the bottom (`bool ⊕ X = X`).
  `bool + bool = bool` is effectively logical OR via write-
  canonicalisation — matches NumPy.
- **Six comparison ops** — `NDArray::eq/ne/lt/le/gt/ge` as `public
  static`, returning bool NDArrays of the broadcast shape. Inputs
  promote before comparing; scalars accepted.
- **`NDArray::where(cond, x, y)`** — 3-operand select. Cond must be
  bool; output dtype = promote(x, y); broadcasts across all three.

Bool flows through every existing surface: reductions
(sum→int64, mean→f64, min/max preserve, argmin/argmax→int64),
shape ops (transpose/reshape/slice/concatenate), and BLAS
(promotes to f64 via the existing rule).

### Decisions locked
- **32** — bool storage layout: canonical 0/1, lenient read, strict write.
- **33** — comparison NaN policy is IEEE 754 (corrects story wording).
  `eq/lt/le/gt/ge` → false on NaN; **`ne` → true on NaN**.
- **34** — bool through reductions follows int promotion logic.
- **35** — comparison ops are method-only (no `==` overload).

## Next pickup — what's natural now

Bool unlocks several follow-ups that were blocked. Rough size order:

### Small (1 sprint each)

- **`any` / `all` reductions on bool arrays** — natural extension of
  the existing reduction surface. New op enum entries + simple
  short-circuiting kernels. Decision 34 already implies bool input;
  output dtype is bool.
- **Bitwise ops** (`&`, `|`, `^`, `~` for elementwise AND/OR/XOR/NOT
  on bool, integer too). Pairs naturally with masks.
- **`unique`** — pulls unique values from a 1-D array. Sort-based
  implementation already on the table from the deferred Story 9
  list.
- **`pad`** — fixed-value or edge padding.

### Medium

- **Boolean indexing** — `$arr[$cond]` returning the elements where
  cond is true. Bigger because it touches `offsetGet` /
  `read_dimension` and needs a return-shape inferred at runtime.
  Naturally pairs with `any`/`all` and bitwise ops above.
- **`take` / `take_along_axis`** — indexed gather along an axis.
  Depends on integer-array indexing (boolean/fancy indexing was
  deferred per system.md). Could ship together with boolean
  indexing.
- **`median` / `percentile` / `quantile`** — already deferred from
  Story 9. Sort-and-pick or partition-based quickselect.

### Larger / more speculative

- **Complex dtypes** (`complex64` / `complex128`) — pairs of
  f32/f64 packed as `(real, imag)`. Unlocks FFT, full eig, full
  SVD interpretation. Heavier lift — needs `zgemm`, `zheev`, etc.
- **`fft` / `ifft`** — depends on complex dtype. Could ship a
  real-input `rfft` variant first.
- **Float16** — ML-relevant, lowest priority unless demand.

### My read

`any`/`all` + bitwise ops + boolean indexing is the next cohesive
sprint — finishes the "bool surface" Story 17 started.
`unique`/`pad`/`median` are small standalone sprints, any order.
Complex dtype work is bigger and speculative; defer until
something concrete (e.g. someone asks for FFT in PHP) surfaces.

### How to start the next session

Tell me which slice to shape and I'll write a user story + sprint
plan in the main thread (no subagent).

## Working state of the build

- Source files changed in sprint 20b: `src/ops.h` (2 new enums:
  `numphp_bitwise_op` and `numphp_logical_op`; 4 new function
  prototypes), `src/ops.c` (`numphp_bitwise` + `numphp_bitwise_not`
  + `numphp_logical` + `numphp_logical_not` patterned on
  `numphp_compare`, plus `bitwise_reject_float` and `read_truthy`
  helpers), `src/ndarray.c` (8 PHP_METHOD wrappers via
  `do_bitwise_method` + `do_logical_method` dispatchers; new
  `arginfo_binop_bool` and `arginfo_unop_bool`), 2 new phpt files
  (`068-bitwise`, `069-logical`), `docs/api/ndarray.md` (new
  Bitwise + Logical sections), `docs/concepts/dtypes.md` (new
  bitwise-vs-logical subsection), `docs/cheatsheet-numpy.md`
  (9 new rows), `docs/system.md` (decisions 39 + 40 written),
  `src/numphp.h` (version → 0.0.25).
- 40 architectural decisions in `docs/system.md`.
- `bench/.venv/` contains numpy 2.4.4. Already gitignored.

### Quality cadence (CI gates that run on every push/PR)

- `build-test` matrix (PHP 8.2/8.3/8.4) — release build.
- `examples` — runs all `examples/*.php`, diffs against `.expected`.
- `valgrind` — phpt under valgrind (`-O0 -g`, `USE_ZEND_ALLOC=0`).
- `sanitizers` — phpt under ASan + UBSan + LSan with
  `detect_leaks=1` and `lsan.supp`.
- `debug-php` — phpt + examples under debug-built PHP with
  `ZEND_RC_DEBUG=1` + `ZEND_ALLOC_DEBUG=1`.
- `coverage` — informational gcov, `continue-on-error: true`.
- `macos` — release build on macOS (Accelerate).

## Known minor follow-ups (not blocking)

- True buffer-mutating in-place methods deferred.
- Multi-axis `slice` deferred; chain `slice()` after `transpose()`.
- Negative `slice` step deferred.
- Strided BLAS deferred.
- 3D+ batched matmul deferred.
- Native int matmul deferred (currently promotes to f64).
- gcov gate still `continue-on-error: true`; flip when v0.1.0
  release prep starts.
- BufferView `writeable=false` is advisory in v1.
- Story 11 Phase C (Arrow IPC) — vendor nanoarrow when revisiting.
- **Sum axis=1 fast path** — would close the ~4× gap; not
  cache-bound so requires SIMD or a different inner-loop shape;
  candidate when there's demand.
- **SIMD intrinsics** for the existing fast paths — closes the last
  ~5% on element-wise and most of the ~4× on axis-0 sum. Portable
  via `<immintrin.h>` for x86 + a NEON path for ARM. Real engineering;
  defer.
- **Bool fast path for arithmetic** — currently bool ⊕ bool always
  takes the slow path so write-canonicalisation runs. Negligible
  perf cost in practice; revisit only if a benchmark surfaces it.

## Where to read the system

- `docs/system.md` — decisions + per-sprint outcomes.
- `docs/status.md` — sprint table.
- `docs/api/`, `docs/concepts/`, `docs/getting-started.md`,
  `docs/cheatsheet-numpy.md` — user-facing.
- `docs/benchmarks.md` — current numbers (refreshed sprint 16).
- `examples/`, `bench/`, `src/`, `tests/` — code.
- `CHANGELOG.md` — `0.0.1` through `0.0.18`.

## How to resume

```bash
cd /home/arciitek/git/numphp
phpize && ./configure && make CFLAGS="-Wall -Wextra -Werror -Wshadow -Wstrict-prototypes -Wmissing-prototypes -O2 -g"
NO_INTERACTION=true make test                       # 67/67 + 1 skipped
bash bench/run.sh                                   # numphp vs NumPy
bash scripts/sanitize.sh                            # ASan + UBSan; on-demand
bash scripts/memcheck.sh                            # valgrind; sprint boundaries
```

## Outstanding ops

- Remote: `git@github.com:erik-ros-devilsberg/NumPHP.git` (origin).

## Known dev-env caveats

- `sudo apt-get install` requires manual user input. Toolchain
  already installed locally.
- System Python is PEP 668-managed; NumPy lives in `bench/.venv/`.
- `make test` interactive prompt: always run with
  `NO_INTERACTION=true`.
