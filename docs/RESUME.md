# Resume Notes — 2026-05-01 (after sprint 13c)

Pick up where this left off next session.

## Where we are

**Version 0.0.13.** Pre-release, iterative. Building toward 0.1.0.

**14 sprints shipped.** Build green at `-Wall -Wextra`; 61/61 phpt + 1
cleanly skipped (FFI ext absent) + the doc-snippet harness running 75
fenced ```php blocks on every PR. **Story 13 fully shipped** (Phases
A + B + C); now resides in `docs/user-stories/done/`.

| # | Sprint | Stories | Status | Version |
|---|--------|---------|--------|---------|
| 1 | project-scaffolding-architecture | 01 | ✓ | 0.0.1 |
| 2 | ndarray-struct-and-creation-api | 02 + 03 | ✓ | 0.0.2 |
| 3 | indexing-and-slicing | 04 | ✓ | 0.0.3 |
| 4 | broadcasting-and-elementwise-ops | 05 + 07 | ✓ | 0.0.4 |
| 5 | shape-manipulation | 06 | ✓ | 0.0.5 |
| 6 | blas-integration | 08 | ✓ | 0.0.6 |
| 7 | stats-and-math-functions | 09 | ✓ | 0.0.7 |
| 8 | linear-algebra-module | 10 | ✓ | 0.0.8 |
| 9 | php-arrays-and-file-io (Story 11 Phase A) | 11A | ✓ | 0.0.9 |
| 10 | buffer-view (Story 11 Phase B) | 11B | ✓ | 0.0.10 |
| 11 | documentation-pass (Story 13 Phase A) | 13A | ✓ | (no bump) |
| 12 | 13b-examples-and-tests (Story 13 Phase B) | 13B | ✓ | 0.0.11 |
| 13 | 15-project-layout (Story 15) | 15 | ✓ | 0.0.12 |
| 14 | 13c-benchmarks (Story 13 Phase C) | 13C | ✓ | 0.0.13 |

**Backlog:** Story 11 Phase C (Arrow IPC) post-1.0, Story 12 (PECL
packaging — currently parked, see below), Story 14 (community +
outreach).

## What just landed (sprint 13c)

The first reproducible numphp-vs-NumPy benchmark.

- New `bench/` directory: `run.php` + `run.py` mirror runners driven
  by a shared `scenarios.json`; `compare.py` joins them into a
  Markdown table; `fingerprint.sh` captures hardware/BLAS/version
  metadata; `run.sh` orchestrates.
- 11 scenarios — element-wise add/multiply, matmul (f64 + f32),
  `sum` along each axis, `fromArray`/`toArray` round-trip,
  `Linalg::solve`, `Linalg::inv`, slice view-creation timer.
- Methodology locked as **decision 30** in `docs/system.md` (7 runs,
  drop slowest, median + min + max; deterministic seed; per-scenario
  fixture allocation excluded except where the fixture is the
  subject).
- NumPy lives in `bench/.venv/` (gitignored). System Python untouched
  — PEP 668-compliant. `bench/run.sh` creates the venv on first run.
- First run committed as `docs/benchmarks.md` with the maintainer's
  hardware fingerprint.
- Smoke test `tests/061-bench-runner-smoke.phpt` — invokes the
  runner via `proc_open`, asserts exit 0 + one JSON record. Doesn't
  lock flaky timing numbers.
- `PHP_NUMPHP_VERSION` → `0.0.13`.
- Story 13 → `done/`.

### What the numbers say

On the maintainer's hardware (Intel m3-6Y30, OpenBLAS via apt,
PHP 8.4, NumPy 2.4.4):

| Scenario | numphp | NumPy | ratio |
|---|---|---|---|
| matmul 1024² f64 | 73.6 ms | 72.4 ms | 1.02× |
| matmul 1024² f32 | 37.4 ms | 37.5 ms | 1.00× |
| Linalg::inv 500² | 23.7 ms | 23.4 ms | 1.01× |
| Linalg::solve 500² | 6.6 ms | 11.1 ms | **0.60×** |
| fromArray 1000² | 21.7 ms | 59.6 ms | **0.36×** |
| toArray 1000² | 20.4 ms | 59.6 ms | **0.34×** |
| slice view | 0.3 µs | 0.5 µs | 0.63× |
| elementwise add 5000² | 415.8 ms | 157.6 ms | 2.64× |
| elementwise mul 5000² | 413.9 ms | 164.7 ms | 2.51× |
| sum axis=1 5000² | 74.7 ms | 19.6 ms | 3.81× |
| sum axis=0 5000² | 314.8 ms | 20.3 ms | 15.48× |

The thesis is validated where it matters: matmul + linalg are at
parity (both call OpenBLAS / LAPACK), and the interop layer is
*faster* than NumPy. Where we lose is in pure element-wise
inner-loop territory NumPy has spent decades vectorising. Axis-0
sum is the worst case (cache-unfriendly, no per-axis kernel yet) —
candidate for a future optimisation sprint when someone has the
itch.

## Next pickup — three viable choices

### Option A: Stop on this milestone

Story 13 fully shipped, version 0.0.13, build clean, real numbers
published. This is a natural pause. Anything beyond is reception
work or follow-on optimisation. Pause here, return when motivated.

### Option B: Story 14 (community + outreach)

Now there's actually something to share — examples, docs, a real
benchmark table. Story 14 is the announcement and outreach work
(PHP internals mailing list, Reddit, NumPy/SciPy community
notice). Substantively it's nontechnical. The benchmark post would
be the centerpiece.

### Option C: Optimisation sprint targeted at the weak spots

- **Element-wise vectorisation** — replace the generic nd-iterator
  inner loop with a SIMD path for contiguous f64 arrays. ~2.5×
  improvement plausible.
- **Per-axis sum kernel** — non-stride-1 axis-0 reduction is 15×
  slower than NumPy. A targeted strided-load kernel could close
  most of the gap.

### Recommendation

Option A or Option B. Option C is real engineering work for a
specific weak spot the project may not need to address before
external use surfaces real-world demand. Don't pre-optimise.

If picking B: `/agile:shape 14-community-and-outreach`. If picking
A: just commit and rest.

**Story 12 (PECL packaging) is deliberately parked** — user
declined to schedule it after we discussed the manifest-maintenance
overhead. Revisit when there's concrete demand for `pecl install
numphp`.

## Working state of the build

- Source files unchanged this sprint — pure additive sprint.
- New top-level directory: `bench/` (with `bench/.venv/` gitignored).
- `PHP_NUMPHP_VERSION` = `0.0.13`.
- 30 architectural decisions captured in `docs/system.md`.

## Known minor follow-ups (not blocking, deferred from earlier sprints)

- True buffer-mutating in-place methods (`$a->addInplace($b)` etc.) deferred.
- Multi-axis `slice` deferred; users chain `slice()` after `transpose()`.
- Negative `slice` step deferred.
- Boolean / fancy indexing not in v1.
- Strided BLAS (passing `lda` for transposed views instead of copying) deferred.
- 3D+ batched matmul deferred.
- Native int matmul deferred (currently promotes to f64).
- gcov gate still `continue-on-error: true`; flip when v0.1.0 release prep starts.
- BufferView `writeable=false` is advisory in v1 (does not actually clear
  `WRITEABLE` on the source NDArray).
- Story 11 Phase C (Arrow IPC): vendoring nanoarrow is the standing
  recommendation when revisiting; user had not committed.
- Element-wise SIMD path and per-axis sum kernel — surfaced by the
  benchmark, candidate for a future targeted optimisation sprint.

## Where to read the system

- `docs/system.md` — the keeper. 30 decisions + per-sprint outcomes.
  Read first.
- `docs/status.md` — sprint table.
- `docs/api/` — complete API reference.
- `docs/concepts/` — five concept guides.
- `docs/getting-started.md` — onboarding.
- `docs/cheatsheet-numpy.md` — NumPy ↔ NumPHP cheat sheet.
- `docs/coverage-audit-2026-05-01.md` — the audit that drove sprint 13b.
- `docs/benchmarks.md` — first benchmark run, hardware-tagged.
- `examples/` — runnable scripts demonstrating real workflows.
- `bench/` — benchmark suite. Re-run with `bash bench/run.sh`.
- `src/` — C source (~7 .c, 7 .h, plus `lapack_names.h`).
- `docs/user-stories/done/` — shipped stories.
  `docs/user-stories/backlog/` — remaining (11 with C deferred, 12, 14).
- `CHANGELOG.md` — `0.0.1` through `0.0.13`.

## How to resume

```bash
cd /home/arciitek/git/numphp
phpize && ./configure && make CFLAGS="-Wall -Wextra -O2 -g"   # clean build
NO_INTERACTION=true make test                                   # 61/61 + 1 skipped (FFI)
bash bench/run.sh                                               # numphp vs NumPy
```

## Outstanding ops

- **No remote configured.** Local commits only.

## Known dev-env caveats

- `sudo apt-get install` requires manual user input — Claude can't
  elevate. Toolchain (`php8.4-dev`, `libopenblas-dev`,
  `liblapack-dev`, `gcovr`) already installed. `valgrind` not
  installed locally; CI runs the valgrind lane in GitHub Actions.
- System Python is PEP 668-managed (no `pip install --user` without
  a venv). Benchmark sprint solved this by putting NumPy in
  `bench/.venv/` — gitignored, isolated from the system.
- `make test` interactive prompt: always run with
  `NO_INTERACTION=true` so no `php_test_results_*.txt` reports get
  auto-saved.
