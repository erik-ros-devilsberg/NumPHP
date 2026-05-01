# Sprint Status

Maintained by the agile plugin. One row per sprint — updated by `/agile:shape`, `/agile:execute`.

| Sprint | Slug | Status | Description |
|--------|------|--------|-------------|
| Project Scaffolding & Architecture Lock-in | project-scaffolding-architecture | completed | Buildable C extension skeleton, four exception classes, CI matrix, decisions in system.md. |
| ndarray Struct & Creation API | ndarray-struct-and-creation-api | completed | Refcounted buffer, ndarray struct, creation methods (zeros/ones/full/eye/arange/fromArray), toArray. |
| Indexing & Slicing | indexing-and-slicing | completed | ArrayAccess + Countable, offsetGet/Set/Exists, slice() axis-0 view, refcounted views share buffer. |
| Broadcasting & Element-wise Ops | broadcasting-and-elementwise-ops | completed | nd-iterator with broadcasting, dtype promotion, add/sub/mul/div, operator overloading. |
| Shape Manipulation | shape-manipulation | completed | reshape, transpose, flatten, squeeze, expandDims, concatenate, stack. |
| BLAS Integration | blas-integration | completed | dot, matmul, inner, outer wired to OpenBLAS (s* and d* paths); int → f64 promotion. |
| Statistical & Mathematical Functions | stats-and-math-functions | completed | Reductions (sum/mean/min/max/std/var/argmin/argmax) with axis+keepdims, Welford, NaN-aware variants, element-wise math, sort/argsort. |
| Linear Algebra Module | linear-algebra-module | completed | `Linalg` static class (inv/det/svd/eig/solve/norm) backed by raw LAPACK; macOS symbol probe fix flipped CI to blocking. |
| Interoperability — Phase A (PHP Arrays & File I/O) | php-arrays-and-file-io | completed | CSV reader/writer + versioned binary load/save; toArray/fromArray round-trip verified. Phases B (FFI BufferView) and C (Arrow IPC) deferred to follow-up sprint(s). |
| Interoperability — Phase B (FFI BufferView) | buffer-view | completed | New `BufferView` class with refcount-protected lifetime over the underlying buffer; `$a->bufferView($writeable = false)`; `$writeable` is advisory in v1. |
| Documentation Pass (Story 13 Phase A) | documentation-pass | completed | API reference (`docs/api/`), getting-started guide, five concept guides, NumPy↔NumPHP cheatsheet, README; decisions 25-27 locked; 48/48 doc snippets verified. |
| Examples & Test-Coverage Audit (Story 13 Phase B) | 13b-examples-and-tests | completed | 5 runnable examples in `examples/` with diff-against-expected CI job; doc snippet harness (`tests/100-doc-snippets.phpt`) makes 75 doc snippets regression-tested; 6 gap-closure phpt tests (055-060); fixed segfault in `fromArray` on mixed-depth siblings. Version 0.0.11. |
| Project Layout (Story 15) | 15-project-layout | completed | C/H sources moved to `src/` via `git mv`; `config.m4` updated with new paths + `PHP_ADD_INCLUDE`; `LICENSE` (BSD 3-Clause, matching NumPy) added; root drops from ~20 tracked files to ~6. Decisions 28-29 locked. No behavioural change. Version 0.0.12. |
| Benchmarks (Story 13 Phase C) | 13c-benchmarks | completed | `bench/` suite with mirrored numphp + NumPy runners; first run committed as `docs/benchmarks.md`. Decision 30 locks the methodology. Honest table: matmul + linalg at parity, interop faster than NumPy, element-wise ~2.5× slower, axis-0 sum 15× slower. Story 13 now fully shipped (A+B+C → `done/`). Version 0.0.13. |
