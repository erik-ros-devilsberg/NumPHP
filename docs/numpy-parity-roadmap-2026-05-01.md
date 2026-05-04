# NumPy Parity — Gap Analysis & Roadmap

*Originally captured 2026-05-01 at v0.0.10 (12 sprints in). **Refreshed
2026-05-04 at v0.0.23** after Sprints 14–22 (incl. all of Story 19).
Filename kept for git-blame continuity.*

This document does two things:

1. Calibrates which gaps look easy but aren't, vs. genuinely 1-day items.
2. Lays out a sprint-by-sprint path from "core foundation" to "comfortable porting from NumPy" — the bar where someone moving code from Python hits a wall maybe once a week instead of once an hour.

The user's intent (recorded 2026-05-01, still standing): we are **not**
releasing until coverage is well past today's level. This document is
the working plan for that push.

---

## Coverage snapshot — 2026-05-04

**~14–16% of NumPy's total function-count surface.** ~85–95 PHP-visible
methods across `NDArray` + `Linalg` + `BufferView` vs. NumPy's ~600
public functions across all submodules. **~50% of "what 80% of working
scientific code actually uses"** — most of NumPy's tail is in
submodules (`ma`, `polynomial`, `lib.financial`, `datetime`) that
real scientific code rarely touches.

**Shipped since the original 2026-05-01 snapshot:**

- Story 13 Phase B (examples + doc-snippet harness + coverage audit) → **0.0.11**.
- Story 15 (project layout: `src/`, BSD 3-Clause LICENSE) → **0.0.12**.
- Story 13 Phase C (benchmarks: `bench/` + numphp vs NumPy table) → **0.0.13**.
- Story 16 (fast paths: element-wise contiguous + axis-0 sum tiled) → **0.0.14**.
- Story 18 — `cumsum` / `cumprod` / `nancumsum` / `nancumprod` → **0.0.17**. [Decision 31]
- Story 17 — `bool` dtype + 6 comparison ops + `where` → **0.0.18**. [Decisions 32–35]
- Story 19 (Build Quality Hardening, all phases):
  - 19a — `-Werror`, `-Wshadow`, `-Wstrict-prototypes`, `-Wmissing-prototypes` → **0.0.19**. [Decision 36]
  - 19b — ASan + UBSan in CI → **0.0.20**. [Decision 37]
  - 19b-fix — `do_operation` compound-assign leak closed; LSan flipped on → **0.0.21**.
  - clean-rule-no-recursive-rm → **0.0.22**.
  - 19c — debug-PHP CI (`ZEND_RC_DEBUG`) → **0.0.23**. [Decision 38]

**Net delta on parity:** ~12 new public methods (4 cumulative + 6 comparisons + `where` + `bufferView` was already counted) + the bool dtype (a category, not a function). The other shipped sprints were quality / infra / docs / perf — no API surface change. Story 19 fully closed; no hardening sprints outstanding before expansion.

---

## What's genuinely a 1-day-each item

The current sprint cadence (1 sprint per focused session) handles each of these in well under a sprint:

- ~~**Trig + hyperbolic functions.**~~ Still missing: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `sinh`, `cosh`, `tanh`, plus `expm1`, `log1p`. Same machinery as `sqrt` / `exp`. One sprint adds the lot.
- **Missing reductions.** ~~`cumsum`, `cumprod`~~ (shipped Sprint 18). Still missing: `prod`, `any`, `all`, `count_nonzero`, `ptp`. Reduction infrastructure exists; mostly copy-paste-modify of the existing `sum` / `argmin` patterns. `any` / `all` need bool input — already unblocked by Sprint 17.
- **Linspace family.** `linspace`, `logspace`, `geomspace`. Half a sprint, trivially.
- ~~**Logical / comparison operators.**~~ **Comparisons shipped (Sprint 17)** as method-only `eq` / `ne` / `lt` / `le` / `gt` / `ge` returning bool NDArrays (decision 35; no `==` overload in v1). Still missing: `logical_and`, `logical_or`, `logical_not`, `logical_xor` and the bitwise siblings (`&`, `|`, `^`, `~`).
- **`median`, `percentile`, `quantile`.** Selection algorithm; one sprint. Build on the existing sort.
- **`np.where`** ✅ shipped Sprint 17. Still missing: `np.select`, `np.choose`, `np.unique`, `np.bincount`, `np.histogram`, `np.searchsorted`, `np.argwhere`, `np.nonzero`. Each individually small. The catch: most are only *useful* once boolean masks exist.

---

## What looks easy but isn't

These are the items where pace estimates from the easy bucket lead to bad calibration.

### Boolean masks + integer fancy indexing

Still *the* big one — and now no longer blocked by bool dtype. It requires:

1. ~~**Bool dtype as a first-class element type.**~~ **Done in Sprint 17** (decisions 32–35). Comparisons return bool NDArrays today; the masks are just sitting there waiting for an indexer that consumes them.
2. **Generalising `offsetGet` / `offsetSet`** to accept array indices, not just integers.
3. **Result shape rules.** Boolean masks return 1-D regardless of input shape; integer fancy indexing has separate rules; combinations of basic + advanced indexing have *more* rules. NumPy's docs on this are notoriously dense for a reason.
4. **View-vs-copy contract.** Fancy indexing always returns a copy in NumPy. NumPHP's view/copy table needs an explicit clause.
5. **Multi-axis combinations** (`a[mask, :, indices]`) which interact with the also-missing multi-axis tuple slicing.

**Estimate (refreshed): 2 sprints.** Down from 2–3 because the bool-dtype precondition shipped. The "no fancy indexing in v1" line in the docs is still load-bearing — it papers over genuine architectural work, but less of it.

### Multi-axis slicing

Unchanged. `a[1:3, 2:5, ::2]` requires PHP-side syntax NumPHP doesn't have. PHP's `ArrayAccess` is one-arg by design. Options:

- **Tuples-as-arrays:** `$a[[range(1,3), range(2,5)]]` — ugly, non-NumPy.
- **Explicit method:** `$a->slice([[1,3], [2,5]])` — readable but verbose.
- **PHP 8.5's potential first-class slice syntax** — speculation.

This isn't a coding problem, it's a PHP-language constraint. Whichever path we pick locks us in. **~1 sprint to ship; the *decision* needs care.**

### Complex dtype

Unchanged. Touches everything:

- Storage layout (interleaved real/imag vs. separate planes).
- Promotion table — every entry expands. **The 5×5 we have today (after bool was added in Sprint 17) becomes 7×7.**
- `Linalg::eig` lifting its real-only restriction (decision 15).
- Every reduction needs to handle complex correctly (no `min`/`max` of complex without an order; argmin/argmax similar).
- All of BLAS's `c*` (complex single) and `z*` (complex double) routines wired in.
- A printer that knows how to render `(1+2j)` consistently.

**Estimate: 3–4 sprints minimum.** Still the v2 line for a reason.

### `tensordot` + `einsum`

Unchanged.

- `tensordot` is a few hundred lines; mechanical generalisation of `matmul`.
- `einsum` has its own subscript parser and dispatch logic. NumPy's implementation is several thousand lines of careful code. The performance optimisations (operand reordering, `np.einsum_path`) are non-trivial.

**Estimate: 2–3 sprints, focused.** Worth doing — every modern ML / scientific library uses einsum. Wait until after the indexing work, since debugging einsum is much easier when fancy indexing is available for cross-checking.

### `np.random`

Unchanged.

- **Basic, ship-fast:** `uniform`, `normal`, `choice`, `shuffle`, `permutation`, seeded `Generator`. PHP has Mersenne Twister and a CSPRNG already; wrapping into NumPHP method form is ~1 sprint.
- **NumPy-compatible:** matching NumPy's `Generator` API with reproducibility across versions. NumPy itself rewrote this in 2019; the bit-stream compat across versions is a real maintenance commitment.

**Estimate: 1 sprint for basic, +2 sprints if we promise version-stable streams.** Recommend basic for v1.0; bit-stream compat as a v1.x item if anyone asks.

### FFT

Unchanged. **Don't write your own.** Vendor [pocketfft](https://github.com/mreineck/pocketfft) — single-header C++, BSD-licensed, the same library NumPy and SciPy switched to. `fft`, `ifft`, `fft2`, `fftn`, real variants (`rfft`/`irfft`).

**Estimate: ~1 sprint** to vendor and wire up. Caveat: pocketfft is C++; need to confirm the build integrates cleanly with our C-only setup, or use the C-port `pocketfft_hdronly` variant.

---

## Sprint-by-sprint roadmap to "comfortable porting from NumPy"

The bar: someone moving code from NumPy hits a wall maybe **once a week**, not once an hour.

| Sprint | Scope | Status | Rationale |
|--------|-------|--------|-----------|
| 13B | Examples + test-coverage audit + gcov filter widen + snippet-as-test CI | ✅ shipped (0.0.11) | Hardening lane. |
| 13C | Benchmarks (`bench/` dir, NumPy comparison) | ✅ shipped (0.0.13) | Hardening lane. |
| 16 | Fast paths (element-wise contig + axis-0 sum tiled) | ✅ shipped (0.0.14) | Surfaced by 13C numbers. |
| 17 | Bool dtype + comparisons + `where` | ✅ shipped (0.0.18) | Was originally Sprint 15 in the old plan; reordered & combined with `where` from old Sprint 19. Three dtype/compare/where things in one sprint. |
| 18 | Cumulative reductions (`cumsum` / `cumprod` + nan variants) | ✅ shipped (0.0.17) | Was Sprint 14 partial — cumulative slice cleaved off because it didn't depend on bool. |
| 19 | Build quality hardening (compiler flags + ASan/UBSan + LSan + debug-PHP) | ✅ shipped (0.0.23) | Three phases (19a/b/c). Closed today. No API surface; sets up the next expansion lane to ship safely. |
| **24** | **`any` / `all` + bitwise ops (`&` / `|` / `^` / `~`) + `logical_and/or/not/xor` + `prod` + `count_nonzero` + `ptp`** | ⏭ next (proposed) | **Finishes the bool surface Story 17 started.** Easy bucket; one sprint covers all. Pairs naturally with masks. |
| 25 | Boolean mask indexing | ⏭ Hard | The architectural lift. Generalises `offsetGet` / `offsetSet`. Bool dtype precondition already shipped (Sprint 17). |
| 26 | Integer fancy indexing + `take` / `take_along_axis` / `put` | ⏭ Medium-hard | Sits on Sprint 25's foundation. |
| 27 | Multi-axis slicing (decide PHP syntax first) | ⏭ Medium | Decision-heavy. Needs a system.md decision before code. |
| 28 | `np.select`, `np.choose`, `np.unique`, `np.bincount`, `np.histogram`, `np.searchsorted`, `np.argwhere`, `np.nonzero` | ⏭ Easy | Each individually trivial; the cluster is one sprint once Sprint 25 lands. |
| 29 | Trig + hyperbolic + `expm1` + `log1p` + `linspace` / `logspace` / `geomspace` | ⏭ Easy | Pure expansion of existing patterns. Confidence-builder. Could move earlier — trivial and unblocked. |
| 30 | `tensordot` + `einsum` | ⏭ Hard | Worth getting right. Big payoff for ML/scientific code. |
| 31 | `np.random` basic (`uniform`, `normal`, `choice`, `shuffle`, `permutation`, seeded `Generator`) | ⏭ Medium | Test fixtures in every NumPy tutorial use this. |
| 32 | FFT via vendored pocketfft | ⏭ Medium | Unlocks signal-processing crowd. |
| 33 | `median` / `percentile` / `quantile` + `qr` / `cholesky` / `pinv` / `lstsq` / `matrix_rank` / `slogdet` | ⏭ Medium | Rounds out reductions and linalg to NumPy's day-to-day list. |

**Total ahead: ~10 sprints** to clear the "comfortable porting" bar. (Down from 13 in the original plan because four expansion sprints — `cumulative`, `bool/compare`, the fast-paths surfacer, the build-quality hardening — already shipped.) At the current cadence (~1 sprint per focused session, ~1–2 sessions per day), that's **1.5–3 calendar weeks** of focused work.

> **Note on numbering:** the sprint numbers above continue past the
> shipped 22 (Sprint 19 was three phases counted as one logical sprint
> in the table even though Sprints 19a/19b/19b-fix/19c each got their
> own version bump). Sprint numbering in `docs/RESUME.md` is more
> granular; this table groups by user-story.

---

## What's deliberately deferred to v2

Items that look like missing v1 features but are actually scope decisions:

- **Complex dtype** (`complex64`, `complex128`). 3–4 sprints, touches everything. v2.0 work.
- **Object dtype, structured dtypes, datetime64 / timedelta64.** Out of scope for a numerical-computing extension; if you need them, you're using the wrong tool.
- **`np.ma` (masked arrays).** Parallel API surface to the entire library. Defer until / unless a real user asks. (The bool-dtype + boolean-mask-indexing work in Sprints 17, 25 covers ~80% of what most users actually want from masks.)
- **`np.polynomial`.** Niche. Build on top of NumPHP if needed.
- **GPU support, DLPack, `__cuda_array_interface__`.** v3 territory.
- **ZTS (thread-safe builds).** Decision 1 — deferred until a thread-safety review of C state.

---

## What this changes about the announcement narrative

Before this roadmap lands, the honest pitch is:

> "NumPHP covers the linear-algebra and elementary-array core of NumPy
> with a real BLAS/LAPACK backend in PHP — bool-dtype boolean
> comparisons, broadcasting, reductions including cumulative, and a
> hardened build (warning-clean, ASan/UBSan/LSan and debug-PHP CI on
> every push)."

After the proposed Sprints 24–33 land, the pitch becomes:

> "NumPHP brings NumPy's API to PHP — n-dimensional arrays,
> broadcasting, BLAS/LAPACK, fancy indexing, FFT, and the random
> module. Complex numbers and a few specialised submodules are v2 work."

Don't pivot the marketing until the work lands. The "NumPy for PHP" framing is a credibility-spend that only pays off when someone's first 10 NumPy tutorials port without hitting a wall.

---

## How this composes with the confidence assessment

[`confidence-2026-05-01.md`](confidence-2026-05-01.md) flagged three gaps: test depth, performance, distribution. The hardening lane has now closed two of those:

- **Test depth:** doc-snippet harness (Sprint 13B), 67/67 phpt + 1 FFI skip, ASan/UBSan/LSan + debug-PHP gates (Sprint 19) — meaningful improvement.
- **Performance:** benchmarks + fast paths (Sprints 13C + 16) brought element-wise to parity with NumPy and axis-0 sum from 15× → 4×.
- **Distribution:** Story 12 (PECL) still parked. Not blocking expansion — distribution can land closer to v0.1.0 release prep.

This document remains an *expansion* roadmap — the hardening sprints listed above are now mostly done. If a future sprint surfaces evidence that a confidence-assessment item has gone *backwards*, pause expansion and fix it. Confidence is a sliding gate.

---

## How to use this document tomorrow

- Read it before shaping any sprint — the dependency order matters (e.g. Sprint 25 unlocks Sprints 26 and 28; Sprint 24 cleans up Sprint 17's loose ends and is unblocked today).
- Re-take both this and the confidence assessment after each sprint. If the "Difficulty" column proved wrong on the last sprint, recalibrate before the next.
- The bool-dtype lift (originally framed as "the first time we expand the dtype list") landed cleanly in Sprint 17 — one sprint, decisions 32–35, no rollback. Treat it as evidence that load-bearing-decision sprints can be sized realistically.
- Don't let the easy-sprint cluster (24, 28, 29) make later sprints feel slower than they are. Velocity will drop in 25, 27, 30 — that's the work being honestly hard, not the cadence breaking.

---

*Author: Claude (Opus 4.7, 1M context). Originally captured during the
wrap-up conversation for `documentation-pass` (Story 13 Phase A) at
the user's request, immediately following [`confidence-2026-05-01.md`](confidence-2026-05-01.md).
Refreshed 2026-05-04 after Story 19 (Build Quality Hardening) closed.*
