# Project Confidence Assessment — 2026-05-01

Snapshot taken right after Story 13 Phase A landed. Twelve sprints shipped; v0.0.10; 53/53 phpt + 1 skipped (FFI). Last commit `4310ac4 sprint 13a` (local-only, no remote).

This document is the candid version of "where do we stand?" — the kind of thing you'd want to read before deciding whether to publish, demo, or tag 1.0.

## High confidence (would defend in a code review)

- **Architecture decisions in `docs/system.md`.** 27 decisions, each with rationale and (mostly) test locks. dtype scope, exception hierarchy, refcounted-buffer model, contiguity flags, BLAS/LAPACK boundary strategy — coherent and consistent with NumPy's choices where they should be, divergent where divergence is justified.
- **BLAS/LAPACK paths.** `matmul`, `dot`, `inv`, `solve`, `svd`, `eig` — linkage works on Linux + macOS (decision 13's symbol-name probe is real engineering, not paper), dispatch rule (`s*` for pure f32, `d*` otherwise) is uniform, asymmetric-matrix tests prevent transpose-trick bugs from hiding.
- **Memory model.** Refcounted buffer + view-as-metadata-shell is clean. Story 11 Phase B's `BufferView` surviving its source `NDArray` proves the design pays off.
- **The phpt test suite as a regression net.** 53 tests, byte-exact diff. Lock tests for round-half (decision 8) and `count()` (decision 27) demonstrate the project values "decision must be testable" over "decision must be perfect."

## Medium confidence (works, but rougher than the surface looks)

- **Test coverage.** 53 tests on 65+ public methods plus operator overloading plus broadcasting plus dtype promotion is thin. gcov gate is still `continue-on-error` — we don't actually know what coverage we have. Story 13 Phase B is supposed to fix this; until then we're documenting an artifact whose quality we haven't measured.
- **Documentation accuracy.** 48/48 snippets verified in Phase A, but that's 48 of many hundreds of fenced code blocks across the docs. The `count()` bug we caught was a *documented divergence we got backwards* — implying there are likely more places where intent and behavior diverge. Phase B's snippet-as-test CI extraction is the right fix; until it lands, the docs carry an unenforced contract.
- **Edge case behavior.** The "common gotchas" lists in the docs imply we know the edges, but most are intent-based, not fuzz-tested. No property tests, no random-shape tests, no NaN/Inf/denormal stress.

## Low confidence (would not bet on it)

- **Performance.** Zero benchmarks run. We assume `matmul` is near-NumPy because both link OpenBLAS — unmeasured. We assume reductions are fast because of pairwise-sum + Welford — unmeasured. We assume `fromArray`/`toArray` is the honest weak spot but haven't quantified the gap. **Story 13 Phase C exists to close this; until it does, anyone asking "is this fast?" gets a shrug.**
- **PECL packaging.** Story 12 is unstarted. Build works on the dev machine. Whether `pecl install numphp` works on a fresh system, on PHP 8.2/8.3/8.4, on a system without OpenBLAS pre-installed — unverified.
- **Public-API stability.** Zero real users. The "release-quality bar" is theoretical until someone outside the project tries to use it.

## The honest meta-point

The project is where you'd expect a one-developer NumPy clone to be after 12 focused sprints:

- Architecture is solid.
- API is plausible.
- Test suite covers the happy path.
- Documentation now exists.

What it lacks is everything that comes from contact with reality: real users, real benchmarks, real install paths on real machines.

## The Story 13 framing was right

The bar from the Story 13 split — *"would I be comfortable if a senior NumPy maintainer read every line of this repo today?"* — is the right bar.

Today's answer:
- **Yes** for the design and the docs.
- **Cautiously yes** for the BLAS/LAPACK paths.
- **No** for the performance claims and the test depth.

Phase B + Phase C + Story 12 are exactly the three gaps. The order of those three is itself a confidence-management decision: **do them before the announcement, not after.**

## Recommended ordering for the road to announcement

1. **Story 13 Phase B (examples + test-coverage audit).** Highest value-per-hour: examples surface API friction that tests don't, and the gcov gate flip + snippet-as-test CI catch silent rot. Confidence in *correctness* moves from "medium" to "high."
2. **Story 13 Phase C (benchmarks).** Confidence in *performance* moves from "low" to whatever the numbers say. The numbers are also the most shareable artifact when announcement time comes.
3. **Story 12 (PECL packaging).** Confidence in *distribution* moves from "low" to "verified on at least the matrix we test." Best done last because (a) it's the most boring and (b) we want the API frozen by then.
4. **Story 11 Phase C (Arrow IPC).** Post-1.0. Speculative until a user asks.
5. **Story 14 (community + outreach).** Post-1.0, contingent on 1-3 above clearing the bar.

## What would change this assessment

A short list of evidence that would move the dial:

- **Confidence in tests:** gcov ≥80% blocking, valgrind clean on full suite in CI, and at least one property-based test pass for broadcasting.
- **Confidence in performance:** `matmul` ≤1.2× NumPy on `[1024,1024]` f64; full reductions ≤1.5× NumPy; the `fromArray`/`toArray` gap quantified and documented honestly.
- **Confidence in distribution:** `pecl install` works in a clean Docker image for at least Ubuntu 24.04 + macOS 14, on PHP 8.2/8.3/8.4.
- **Confidence in API:** at least one user outside the project tries it and reports either "this works" or specific friction we can fix.

## How to use this document tomorrow

Read it as the "what worries me" pass before deciding the next sprint. If anything in the **Low confidence** section is still low after tomorrow's work, the next-next sprint should be one that closes that gap rather than adding scope. The pre-1.0 window is for tightening, not expanding.

Re-take this assessment after each phase. Compare confidence levels — if they don't move, the sprint didn't accomplish what it should have.

---

*Author: Claude (Opus 4.7, 1M context). Captured during the wrap-up conversation for `documentation-pass` (Story 13 Phase A) at the user's request.*
