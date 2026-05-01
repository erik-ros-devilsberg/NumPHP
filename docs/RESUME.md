# Resume Notes — 2026-05-01 (after sprint 18)

## Where we are

**Version 0.0.18.** Pre-release, iterative. Building toward 0.1.0.

**17 sprints shipped.** Build green at `-Wall -Wextra`; 67/67 phpt + 1
FFI skip + doc-snippet harness running ~75+ fenced ```php blocks.
Last two sprints added cumulative reductions (sprint 17) and the
`bool` dtype + comparisons + `where` (sprint 18).

| # | Sprint | Stories | Version |
|---|--------|---------|---------|
| 1-13 | (see CHANGELOG) | | 0.0.1 → 0.0.12 |
| 14 | 13c-benchmarks (Story 13 Phase C) | 13C | 0.0.13 |
| 15 | 16-fastpath-optimizations (Story 16) | 16 | 0.0.14 |
| 16 | (skipped — version numbering rule changed mid-flight) | — | — |
| 17 | 17-cumsum-cumprod (Story 18) | 18 | 0.0.17 |
| 18 | 18-bool-and-comparisons (Story 17) | 17 | 0.0.18 |

**Backlog:** Story 11 Phase C (Arrow IPC) post-1.0, Story 12 (PECL
packaging — parked), Story 14 (community + outreach).

## What just landed (sprint 18)

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

- Source files changed in sprint 18: `src/ndarray.h` (enum +
  NUMPHP_BOOL), `src/ndarray.c` (dtype helpers, read/write,
  fromArray/toArray, eye/arange, fast-path predicate excludes
  bool, slow-path NUMPHP_BOOL case in arithmetic, comparison +
  where PHP_METHOD wrappers, arginfo, method table), `src/nditer.h`
  (typed bool reads), `src/nditer.c` (5×5 promotion table),
  `src/io.c` (dtype byte check widened, format_cell bool case),
  `src/ops.h` (compare op enum, numphp_compare/where prototypes),
  `src/ops.c` (numphp_compare + numphp_where + bool entries in
  reduce_out_dtype/cumulative_out_dtype), `src/numphp.h` (version).
- 35 architectural decisions in `docs/system.md` (32–35 added
  this sprint).
- `bench/.venv/` contains numpy 2.4.4. Already gitignored.

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
phpize && ./configure && make CFLAGS="-Wall -Wextra -O2 -g"
NO_INTERACTION=true make test                       # 67/67 + 1 skipped
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
