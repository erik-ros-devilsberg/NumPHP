---
story: Documentation, Examples, Tests & Benchmarks
created: 2026-04-29
updated: 2026-05-01
phases:
  - A: shipped 2026-05-01 — documentation prose (API ref, concepts, getting-started, cheatsheet, README)
  - B: shipped 2026-05-01 — 5 examples with CI diff job, doc-snippet harness, 6 gap-closure phpt tests, fromArray segfault fix
  - C: shipped 2026-05-01 — bench/ suite + first run as docs/benchmarks.md; methodology locked as decision 30
---

> Part of [Epic: NumPHP](epic-numphp.md)

## Description

Bring the extension to release quality before any public announcement. The 1.0 release will draw scrutiny from PHP internals folks, NumPy/SciPy users with strong opinions, and people looking for reasons to dismiss "yet another numerical library." Every visible surface — docs, examples, tests, benchmark numbers — must hold up to a careful third-party read.

This story covers four work streams, all aimed at pre-release quality. **Community outreach is deliberately out of scope** and lives in Story 14.

### Documentation
- API reference in php.net format. Every public method on `NDArray`, `Linalg`, `BufferView`, plus the four exception classes, with parameter types, return type, throws-list, and a minimal example per method.
- Getting-started guide: install → first array → indexing → broadcasting → save/load. End-to-end in one read.
- Concept guides for the non-obvious bits: dtype promotion table, broadcasting rules, view-vs-copy semantics, NaN policy in reductions, the round-half divergence from NumPy (decision 8).
- A "porting from NumPy" cheatsheet — side-by-side NumPy ↔ numphp for the 30–50 most common operations.

### Examples
- 5–10 runnable example scripts in `examples/` covering canonical workflows: linear regression, k-means, image-as-array manipulation, simple time-series, CSV → analysis → save.
- Each example self-contained, runnable via `php examples/<name>.php`, with expected output checked in alongside.
- Examples double as integration tests — wire them into CI so regressions break the build.

### Tests (release-quality pass)
- Audit the existing 53 phpt tests for coverage gaps. Areas likely under-tested: error paths (every exception class should have at least one test that triggers it), edge shapes (0-D arrays, single-element arrays, very large 1-D arrays), dtype boundary cases (int overflow, denormals, NaN/Inf propagation through every reduction).
- Flip the gcov gate from `continue-on-error: true` to blocking at a target threshold. 80% is the original target; revisit based on what's reasonable after the audit.
- Confirm the valgrind lane runs the full phpt suite on every PR.
- Document any deliberate test exclusions in `docs/system.md`.

### Benchmarks
The benchmark post is the highest-leverage release artifact — numbers are what get shared.

Required scenarios:
- Element-wise add/multiply on `[10_000, 10_000]` f64.
- `matmul` on `[1024, 1024]` f64 (BLAS path — should be near-NumPy since both link OpenBLAS).
- Reduction `sum` along each axis on `[10_000, 10_000]` f64.
- `fromArray` / `toArray` round-trip on a `[1000, 1000]` f64 array (the honest weak spot — PHP-array ↔ buffer copy is unavoidable interop overhead).

Suggested additions:
- `Linalg::inv` / `solve` on a `[500, 500]` f64 matrix (LAPACK path).
- Slicing + view creation cost (should be O(1) — proves the no-copy claim).
- f32 vs f64 matmul on the same shape (proves the dtype path is real).

Per-scenario output: numphp time, NumPy time, ratio, hardware fingerprint, BLAS variant, PHP version, NumPy version. Reproducibility is the credibility multiplier — publish the script.

A `bench/` directory in the repo with a single entry-point script (`php bench/run.php` or similar) that produces the table. Use `hrtime(true)` for timing; multiple runs with median + IQR; warm up before measuring.

## Acceptance Criteria

- API reference is complete: every public method documented with signature, params, returns, throws, and example.
- Getting-started guide takes a new user from zero to a working save/load round-trip without external lookups.
- Concept guides cover dtype promotion, broadcasting, view-vs-copy, NaN policy, and the round-half divergence.
- NumPy ↔ numphp cheatsheet covers ≥30 operations.
- `examples/` contains ≥5 runnable scripts, each with checked-in expected output, all wired into CI.
- Test coverage gaps from the audit are filled or explicitly documented as deferred.
- gcov gate is blocking in CI at the chosen threshold.
- Valgrind lane runs the full suite without leaks or invalid reads on every PR.
- `bench/` produces a reproducible numphp-vs-NumPy table covering the required scenarios; raw output is checked in for the v1 hardware baseline.
- Benchmark post draft exists in the repo (e.g. `docs/benchmarks-v1.md`), even if external publication waits for Story 14.

## Out of scope (moved to Story 14)

- PHP internals mailing list announcement.
- Reddit / Hacker News posts.
- RubixML team outreach.
- Publishing the benchmark post externally.
- Any social / public-facing communication.

The bar for moving from Story 13 → Story 14 is "would I be comfortable if a senior NumPy maintainer read every line of this repo today?"
