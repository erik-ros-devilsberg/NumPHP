# NumPy Parity — Gap Analysis & Roadmap

*Snapshot: 2026-05-01. Twelve sprints shipped, v0.0.10. Captured at the user's request after a confidence assessment showed roughly 10–12% function-count coverage of NumPy and ~50% coverage of "what 80% of scientific code needs."*

This document does two things:

1. Calibrates which gaps look easy but aren't, vs. genuinely 1-day items.
2. Lays out a sprint-by-sprint path from "core foundation" to "comfortable porting from NumPy" — the bar where someone moving code from Python hits a wall maybe once a week instead of once an hour.

The user's intent (recorded 2026-05-01): we are **not** releasing until coverage is well past today's level. This document is the working plan for that push.

---

## What's genuinely a 1-day-each item

The current sprint cadence (1 sprint per focused session) handles each of these in well under a sprint:

- **Trig + hyperbolic functions.** `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `sinh`, `cosh`, `tanh`, plus `expm1`, `log1p`. Same machinery as `sqrt` / `exp`. One sprint adds the lot.
- **Missing reductions.** `prod`, `cumsum`, `cumprod`, `any`, `all`, `count_nonzero`. The reduction infrastructure already exists; mostly copy-paste-modify of the existing `sum` / `argmin` patterns.
- **Linspace family.** `linspace`, `logspace`, `geomspace`. Half a sprint, trivially.
- **Logical / comparison operators.** `>`, `<`, `==`, `!=`, `>=`, `<=`, `logical_and`, `logical_or`, `logical_not`. Mechanical — but **forces the bool dtype decision** (currently deferred per decision 2). Once bool exists, comparisons return bool arrays naturally.
- **`median`, `percentile`, `quantile`.** Selection algorithm; one sprint. Build on the existing sort.
- **`np.where`, `np.select`, `np.choose`, `np.unique`, `np.bincount`, `np.histogram`.** Each individually small. The catch: they're only *useful* once boolean masks exist.

---

## What looks easy but isn't

These are the items where pace estimates from the easy bucket lead to bad calibration.

### Boolean masks + integer fancy indexing

This is *the* big one. It's not "add a method" — it requires:

1. **Bool dtype as a first-class element type** (currently deferred per decision 2). Comparison operators have nowhere to put their results without it.
2. **Generalising `offsetGet` / `offsetSet`** to accept array indices, not just integers.
3. **Result shape rules.** Boolean masks return 1-D regardless of input shape; integer fancy indexing has separate rules; combinations of basic + advanced indexing have *more* rules. NumPy's docs on this are notoriously dense for a reason.
4. **View-vs-copy contract.** Fancy indexing always returns a copy in NumPy. NumPHP's view/copy table needs an explicit clause.
5. **Multi-axis combinations** (`a[mask, :, indices]`) which interact with the also-missing multi-axis tuple slicing.

**Estimate: 2–3 sprints.** The "no fancy indexing in v1" line in the docs is load-bearing — it papers over genuine architectural work.

### Multi-axis slicing

`a[1:3, 2:5, ::2]` requires PHP-side syntax NumPHP doesn't have. PHP's `ArrayAccess` is one-arg by design. Options:

- **Tuples-as-arrays:** `$a[[range(1,3), range(2,5)]]` — ugly, non-NumPy.
- **Explicit method:** `$a->slice([[1,3], [2,5]])` — readable but verbose.
- **PHP 8.5's potential first-class slice syntax** — speculation.

This isn't a coding problem, it's a PHP-language constraint. Whichever path we pick locks us in. **~1 sprint to ship; the *decision* needs care.**

### Complex dtype

Touches everything:

- Storage layout (interleaved real/imag vs. separate planes).
- Promotion table — every entry expands.
- `Linalg::eig` lifting its real-only restriction (decision 15).
- Every reduction needs to handle complex correctly (no `min`/`max` of complex without an order; argmin/argmax similar).
- All of BLAS's `c*` (complex single) and `z*` (complex double) routines wired in.
- A printer that knows how to render `(1+2j)` consistently.

**Estimate: 3–4 sprints minimum.** This is the v2 line for a reason. Holding it for a v2.0 push lets v1 stay coherent.

### `tensordot` + `einsum`

- `tensordot` is a few hundred lines; mechanical generalisation of `matmul`.
- `einsum` has its own subscript parser and dispatch logic. NumPy's implementation is several thousand lines of careful code. The performance optimisations (operand reordering, `np.einsum_path`) are non-trivial.

**Estimate: 2–3 sprints, focused.** Worth doing — every modern ML / scientific library uses einsum. Wait until after the indexing work, since debugging einsum is much easier when fancy indexing is available for cross-checking.

### `np.random`

Two scopes here:

- **Basic, ship-fast:** `uniform`, `normal`, `choice`, `shuffle`, `permutation`, seeded `Generator`. PHP has Mersenne Twister and a CSPRNG already; wrapping into NumPHP method form is ~1 sprint.
- **NumPy-compatible:** matching NumPy's `Generator` API with reproducibility across versions. NumPy itself rewrote this in 2019; the bit-stream compat across versions is a real maintenance commitment.

**Estimate: 1 sprint for basic, +2 sprints if we promise version-stable streams.** Recommend basic for v1.0; bit-stream compat as a v1.x item if anyone asks.

### FFT

**Don't write your own.** Vendor [pocketfft](https://github.com/mreineck/pocketfft) — single-header C++, BSD-licensed, the same library NumPy and SciPy switched to. `fft`, `ifft`, `fft2`, `fftn`, real variants (`rfft`/`irfft`).

**Estimate: ~1 sprint** to vendor and wire up. Caveat: pocketfft is C++; need to confirm the build integrates cleanly with our C-only setup, or use the C-port `pocketfft_hdronly` variant.

---

## Sprint-by-sprint roadmap to "comfortable porting from NumPy"

The bar: someone moving code from NumPy hits a wall maybe **once a week**, not once an hour. Inspired by the "would a senior NumPy maintainer be comfortable?" framing from Story 13.

| Sprint | Scope | Difficulty | Rationale |
|--------|-------|------------|-----------|
| 13B | Examples + test-coverage audit + gcov gate flip + snippet-as-test CI | Medium | Already on the books. Closes the test-depth gap from the confidence assessment. |
| 13C | Benchmarks (`bench/` dir, NumPy comparison, draft post) | Medium | Already on the books. Closes the performance-claim gap. Numbers may surface optimisation work that should land now, not later. |
| 12 | PECL packaging | Medium | Already on the books. Best done after Phase B/C so the API is frozen. |
| 14 | Trig + hyperbolic + missing reductions (`prod`, `cumsum`, `cumprod`, `any`, `all`, `count_nonzero`) + `linspace` / `logspace` / `geomspace` | Easy | Pure expansion of existing patterns. Confidence-builder. |
| 15 | Bool dtype + comparison operators (`>`, `<`, `==`, …) + `logical_and` / `or` / `not` | Medium | Touches the dtype promotion table — first time we've expanded that. Forces decisions on bool's role. |
| 16 | Boolean mask indexing | Hard | The architectural lift. Generalises `offsetGet` / `offsetSet`. |
| 17 | Integer fancy indexing + `take` / `put` | Medium-hard | Sits on Sprint 16's foundation. |
| 18 | Multi-axis slicing (decide PHP syntax first) | Medium | Decision-heavy. Needs a system.md decision before code. |
| 19 | `np.where`, `np.select`, `np.choose`, `np.unique`, `np.bincount`, `np.histogram`, `np.searchsorted`, `np.argwhere`, `np.nonzero` | Easy | Each individually trivial; the cluster is one sprint once Sprint 16 lands. |
| 20 | `tensordot` + `einsum` | Hard | Worth getting right. Big payoff for ML/scientific code. |
| 21 | `np.random` basic (`uniform`, `normal`, `choice`, `shuffle`, `permutation`, seeded `Generator`) | Medium | Test fixtures in every NumPy tutorial use this. |
| 22 | FFT via vendored pocketfft | Medium | Unlocks signal-processing crowd. |
| 23 | `median` / `percentile` / `quantile` + `qr` / `cholesky` / `pinv` / `lstsq` / `matrix_rank` / `slogdet` | Medium | Rounds out reductions and linalg to NumPy's day-to-day list. |

**Total: 13 more sprints** beyond what's already on the books, to clear the "comfortable porting" bar. At the current cadence (~1 sprint per focused session, ~1–2 sessions per day), that's **2–4 calendar weeks** of focused work.

---

## What's deliberately deferred to v2

Items that look like missing v1 features but are actually scope decisions:

- **Complex dtype** (`complex64`, `complex128`). 3–4 sprints, touches everything. v2.0 work.
- **Object dtype, structured dtypes, datetime64 / timedelta64.** Out of scope for a numerical-computing extension; if you need them, you're using the wrong tool.
- **`np.ma` (masked arrays).** Parallel API surface to the entire library. Defer until / unless a real user asks.
- **`np.polynomial`.** Niche. Build on top of NumPHP if needed.
- **GPU support, DLPack, `__cuda_array_interface__`.** v3 territory.
- **ZTS (thread-safe builds).** Decision 1 — deferred until a thread-safety review of C state.

---

## What this changes about the announcement narrative

Before this roadmap lands, the honest pitch is:

> "NumPHP covers the linear-algebra and elementary-array core of NumPy with a real BLAS/LAPACK backend in PHP."

After Sprints 14–23 land, the pitch becomes:

> "NumPHP brings NumPy's API to PHP — n-dimensional arrays, broadcasting, BLAS/LAPACK, fancy indexing, FFT, and the random module. Complex numbers and a few specialised submodules are v2 work."

Don't pivot the marketing until the work lands. The "NumPy for PHP" framing is a credibility-spend that only pays off when someone's first 10 NumPy tutorials port without hitting a wall.

---

## How this composes with the confidence assessment

[`confidence-2026-05-01.md`](confidence-2026-05-01.md) flagged three gaps: test depth, performance, distribution. This roadmap addresses **none of those directly** — it's an *expansion* roadmap, not a hardening roadmap.

That's why Sprints 13B, 13C, and 12 (already on the books) come first. They harden what we have. Only then is it worth expanding.

If a sprint from this roadmap surfaces evidence that a confidence-assessment item has gone *backwards* (e.g. fancy indexing exposes a refcount bug, or trig functions have terrible performance vs. NumPy), pause expansion and fix it. Confidence is a sliding gate — the bar is "every shipped sprint either holds or improves the confidence picture."

---

## How to use this document tomorrow

- Read it before shaping any sprint — the dependency order matters (e.g. Sprint 16 unlocks Sprint 17 and Sprint 19; Sprint 15 unlocks Sprint 16).
- Re-take both this and the confidence assessment after each sprint. If the roadmap's "Difficulty" column proved wrong on the last sprint, recalibrate before the next.
- Bool dtype (Sprint 15) is the first time we expand a load-bearing architectural decision (the dtype list in decision 2). Treat it carefully.
- Don't let the easy-sprint cluster (14, 19) make later sprints feel slower than they are. Velocity will drop in 16, 18, 20 — that's the work being honestly hard, not the cadence breaking.

---

*Author: Claude (Opus 4.7, 1M context). Captured during the wrap-up conversation for `documentation-pass` (Story 13 Phase A) at the user's request, immediately following [`confidence-2026-05-01.md`](confidence-2026-05-01.md).*
