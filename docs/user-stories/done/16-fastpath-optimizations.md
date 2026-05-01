---
story: Fastpath optimisations — element-wise contiguous + axis-0 sum tiled kernel
created: 2026-05-01
---

> Part of [Epic: NumPHP](epic-numphp.md)

## Description

The 0.0.13 benchmark surfaced two weak spots with clear root causes
and clear fixes:

1. **Element-wise add/multiply on contiguous same-dtype arrays — 2.5×
   slower than NumPy.** The hot loop (`src/ndarray.c::do_binary_op_core`)
   pays for: (a) a `switch` on output dtype per element, (b) a function
   call to `numphp_read_f64` per input read (handles mixed-dtype
   broadcasting), (c) multi-dim iterator advance per element walking
   the strides array. None of these are needed when both inputs are
   C-contiguous and same dtype as output — the common case.

2. **Axis-0 sum on a 2-D contiguous source — 15× slower than NumPy.**
   The hot path (`src/ops.c::reduce_line` → `pairwise_sum_f64`) sums
   each output column independently with stride = `ncols * sizeof(double)`
   between adjacent reads. Each read jumps a full row (~40 KB on a
   5000-column array), causing repeated cache misses. NumPy uses a
   tiled kernel that processes K columns in lockstep so a single
   row-line read serves K accumulators.

Both fixes are local kernel additions, no API change, no architectural
work. The existing slow paths stay as fallback for the cases the
fast paths don't cover (broadcasting, mixed dtype, non-contiguous
sources, axes other than 0).

## Acceptance Criteria

- Element-wise fast path: when `a`, `b`, `out` are C-contiguous,
  shapes equal (no broadcasting), and `a->dtype == b->dtype ==
  out_dtype`, dispatch to a flat typed-pointer loop. The compiler
  must auto-vectorise it at `-O2`.
- Axis-0 sum fast path: when source is 2-D, C-contiguous, dtype f64
  or f32, axis=0, no NaN-skip, dispatch to a tiled kernel that
  processes the columns in strips of K (start with K=32). Pairwise
  recursion structure preserved per column so output is bit-identical
  to the slow path.
- All 61 existing phpt tests pass unchanged. No new behavioural
  surface — these are *internal* speed-ups, not new APIs.
- Slow paths unchanged for everything outside the fast-path predicates.
  Mixed-dtype, broadcasting, non-contiguous, axis≠0 reduction all
  still go through the original code.
- Benchmark re-run committed as `docs/benchmarks-2026-05-01-postopt.md`
  (or update `docs/benchmarks.md` in place — choice during sprint
  shape). Element-wise ratios should drop from ~2.5× to ≤1.5×;
  axis-0 sum from ~15× to ≤4×. Numbers below those targets are a
  sprint failure.
- Version bumped (likely 0.0.14).

## Out of scope

- SIMD intrinsics (`<immintrin.h>`, `__m256d`, etc.). Compiler
  auto-vectorisation only — keeps the code portable across x86, ARM,
  Apple Silicon. SIMD is a future optimisation if profiling shows
  auto-vec missed the boat.
- Axis-1 sum optimisation (3.81× slower currently). Not catastrophic;
  a separate fast path would help but it's an additive sprint.
- Fast paths for other reductions (`mean`, `var`, `std`, `min`, `max`)
  — they have different inner-loop shape; defer.
- Axis-N reduction generalisation. Stay laser-focused on axis=0 of 2-D
  contiguous; that's the worst case from the benchmark.
- New dtypes, new functions. Those are the stories that follow this
  one.

## Notes

The element-wise fast path may also expose a small behavioural
question: floating-point sum order. For `a + b` with no broadcasting
and no dtype-mixing, the per-element result is identical regardless
of iteration order — `+` is element-wise pairwise, no associativity
issue. So bit-equality to the slow path is guaranteed.

For axis-0 sum, the existing slow path uses `pairwise_sum_f64` per
column. The tiled kernel must preserve the same recursion structure
(split rows in half, recurse, sum two halves) so each column's sum
is bit-identical to its pre-optimisation value. Tests `032-…` and
`038-…` lock this — re-running them with the fast path enabled
catches any precision drift.
