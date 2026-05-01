# Resume Notes — 2026-05-01 (after sprint 16)

## Where we are

**Version 0.0.14.** Pre-release, iterative. Building toward 0.1.0.

**15 sprints shipped.** Build green at `-Wall -Wextra`; 61/61 phpt + 1
FFI skip + doc-snippet harness running 75 fenced ```php blocks. Last
sprint closed the two clear weak spots from the benchmark.

| # | Sprint | Stories | Version |
|---|--------|---------|---------|
| 1-13 | (see CHANGELOG) | | 0.0.1 → 0.0.12 |
| 14 | 13c-benchmarks (Story 13 Phase C) | 13C | 0.0.13 |
| 15 | 16-fastpath-optimizations (Story 16) | 16 | 0.0.14 |

**Backlog:** Story 11 Phase C (Arrow IPC) post-1.0, Story 12 (PECL
packaging — parked), Story 14 (community + outreach).

## What just landed (sprint 16)

Two kernel additions, no API change, no SIMD intrinsics.

- **Element-wise contiguous fast path** in `src/ndarray.c::do_binary_op_core`.
  Predicate: both inputs C-contig, same dtype as output, identical
  shape (no broadcasting). Falls through to a flat typed-pointer
  loop the compiler auto-vectorises at `-O2`.
- **Axis-0 sum tiled kernel** in `src/ops.c::numphp_reduce`.
  Predicate: 2-D C-contig f32/f64, axis=0, no NaN-skip. Strips of 32
  columns; pairwise recursion on rows preserved per column → output
  is bit-identical to the slow path.

Slow paths kept for everything outside the predicates (broadcasting,
mixed dtype, non-contig, axis≠0, integer reductions, NaN-skip).

### Numbers (vs NumPy on the maintainer's hardware)

| Scenario | Before (0.0.13) | After (0.0.14) | Verdict |
|---|---|---|---|
| elementwise add 5000² | 2.64× | **1.01×** | parity |
| elementwise mul 5000² | 2.51× | **1.05×** | parity |
| sum axis=0 5000² | 15.48× | **4.40×** | 3.5× faster |
| sum axis=1 5000² | 3.81× | 4.28× | unchanged within noise |
| matmul 1024² f64 | 1.02× | 0.96× | parity |
| matmul 1024² f32 | 1.00× | 1.00× | parity |
| Linalg::solve 500² | 0.60× | 0.63× | faster than NumPy |
| Linalg::inv 500² | 1.01× | 1.12× | parity |
| fromArray 1000² | 0.36× | 0.38× | faster than NumPy |
| toArray 1000² | 0.34× | 0.34× | faster than NumPy |
| slice (view) | sub-µs | sub-µs | parity |

**Effect on the project's thesis:** numphp is now within 5% of
NumPy on common element-wise ops, faster than NumPy on the interop
boundary, at parity on the heavy BLAS/LAPACK lifting, and within
~4× on axis reductions. The remaining ~4× gap is no longer
cache-bound — closing it requires SIMD intrinsics (deferred).

## Next pickup — extra functions and datatypes

User direction at the end of sprint 16: "after that, we will focus
on some of the extra functions and datatypes."

Candidates worth thinking about (not yet shaped — pick what makes
sense when starting):

### Datatypes
- **`bool`** — arrays of true/false. Currently absent from the dtype
  list. Would unlock proper boolean-result comparisons (`$a > $b`),
  boolean indexing later, and a "where" function. Decision 2 in
  `system.md` deliberately deferred bool to v1.
- **`complex64` / `complex128`** — pairs of f32/f64 packed as
  `(real, imag)`. Unlocks FFT, eigendecomposition with complex
  results (currently `eig` throws on complex eigenvalues, decision
  15), full SVD interpretation. Heavier lift — needs new BLAS/
  LAPACK code paths (`zgemm`, `zheev`, etc.).
- **`float16`** — half precision. ML-relevant. Probably lowest
  priority unless there's specific demand.

### Functions
- **Comparison ops** (`==`, `!=`, `<`, `>`, `<=`, `>=`) returning
  boolean arrays — depends on bool dtype.
- **`where`** — three-argument NDArray::where(cond, x, y). Depends
  on bool.
- **`cumsum` / `cumprod`** — cumulative reductions. Useful for
  time-series and probability work.
- **`median` / `percentile` / `quantile`** — already deferred per
  Story 9 outcome. Implementation needs a sort-and-pick or
  partition-based quickselect.
- **`unique`** — pulls unique values from a 1-D array.
- **`pad`** — fixed-value or edge padding.
- **`take` / `take_along_axis`** — indexed gather along an axis.
  Depends on integer-array indexing (boolean/fancy indexing was
  deferred per system.md).
- **`fft` / `ifft`** — Fourier transforms. Heavy lift; depends on
  complex dtype. Could ship a real-input (`rfft`) variant first.

My read: **bool dtype + comparison ops + `where`** is one cohesive
sprint that unlocks several follow-ups. `cumsum`/`cumprod` is a
small standalone sprint. The complex dtype work is bigger and more
speculative — defer until something concrete (e.g. someone wants
to do FFT in PHP) surfaces.

### How to start the next session

Tell me which slice of "extra functions and datatypes" to shape and
I'll write a user story + sprint plan in the main thread (no
subagent).

## Working state of the build

- Source files changed this sprint: `src/ndarray.c` (element-wise
  fast path), `src/ops.c` (axis-0 kernel + helpers), `src/numphp.h`
  (version).
- 30 architectural decisions in `docs/system.md`; sprint 16 didn't
  add a new decision (internal optimisation, not architecture).
- `bench/.venv/` contains numpy 2.4.4. Already gitignored.

## Known minor follow-ups (not blocking)

- True buffer-mutating in-place methods deferred.
- Multi-axis `slice` deferred; chain `slice()` after `transpose()`.
- Negative `slice` step deferred.
- Boolean / fancy indexing not in v1.
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

## Where to read the system

- `docs/system.md` — decisions + per-sprint outcomes.
- `docs/status.md` — sprint table.
- `docs/api/`, `docs/concepts/`, `docs/getting-started.md`,
  `docs/cheatsheet-numpy.md` — user-facing.
- `docs/benchmarks.md` — current numbers (refreshed sprint 16).
- `examples/`, `bench/`, `src/`, `tests/` — code.
- `CHANGELOG.md` — `0.0.1` through `0.0.14`.

## How to resume

```bash
cd /home/arciitek/git/numphp
phpize && ./configure && make CFLAGS="-Wall -Wextra -O2 -g"
NO_INTERACTION=true make test                       # 61/61 + 1 skipped
bash bench/run.sh                                   # numphp vs NumPy
```

## Outstanding ops

- No remote configured. Local commits only.

## Known dev-env caveats

- `sudo apt-get install` requires manual user input. Toolchain
  already installed locally.
- System Python is PEP 668-managed; NumPy lives in `bench/.venv/`.
- `make test` interactive prompt: always run with
  `NO_INTERACTION=true`.
