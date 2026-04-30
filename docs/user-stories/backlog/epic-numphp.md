# Epic: NumPHP — NumPy-equivalent PHP Extension

## Goal
A native PHP C extension providing n-dimensional array primitives, vectorised operations, and BLAS/LAPACK bindings — making PHP viable for numerical computing and data science workloads.

## Architecture
- **Backend:** OpenBLAS / LAPACK (C/Fortran) — unchanged from NumPy
- **Facade:** PHP C extension replacing NumPy's Python layer
- **Userland:** `NDArray` class (+ `Linalg` static class) exposed to PHP developers

The extension is the facade. All C code — method registration, typed loops, dtype dispatch, the nd-iterator — sits between PHP userland and the BLAS/LAPACK backends.

## Cross-cutting decisions

These are locked in [Story 1](01-project-scaffolding.md) and referenced everywhere else:

- **PHP target:** 8.2+, NTS only. ZTS deferred.
- **dtypes:** `float32`, `float64`, `int32`, `int64` for v1.
- **dtype promotion table:** see Story 1.
- **Exception hierarchy:** `\NDArrayException` ← `\ShapeException`, `\DTypeException`, `\IndexException`.
- **Memory model:** refcounted underlying buffer (Story 2) decoupled from the array shell; no copy-on-write in v1.
- **Contiguity:** `flags` bitfield on every array (`C_CONTIGUOUS`, `F_CONTIGUOUS`, `WRITEABLE`).
- **Error policy:** float divzero = IEEE; int divzero = `\DivisionByZeroError`; OOB index = `\IndexException`.

## Stories

1. [Project Scaffolding & Architecture Decisions](../done/01-project-scaffolding.md) ✓
2. [ndarray Data Structure](../done/02-ndarray-data-structure.md) ✓
3. [Array Creation API](../done/03-array-creation-api.md) ✓
4. [Indexing & Slicing](../done/04-indexing-and-slicing.md) ✓
5. [Element-wise Operations](../done/05-elementwise-operations.md) ✓
6. [Shape Manipulation](../done/06-shape-manipulation.md) ✓
7. [Broadcasting & nd-iterator](../done/07-broadcasting.md) ✓
8. [BLAS Integration](../done/08-blas-integration.md) ✓
9. [Statistical & Mathematical Functions](09-stats-and-math-functions.md)
10. [Linear Algebra Module](10-linear-algebra-module.md)
11. [Interoperability](11-interoperability.md)
12. [PECL Packaging & Distribution](12-pecl-packaging.md)
13. [Documentation, Examples, Tests & Benchmarks](13-docs-and-benchmarks.md)
14. [Community Seeding & Outreach](14-community-and-outreach.md)

## Dependency Map

```
1 → 2 → 3 → 4 → 7 → 5
                ↓    ↓
                6    8 → 10
                ↓
                9
2 → 11 (phases A → B done; C deferred post-1.0)
All → 12
13 (release-quality pass — docs, examples, tests, benchmarks)
13 → 14 (outreach — only after release-quality bar is met)
```

Note the **7 → 5** order: the nd-iterator from Story 7 is built before element-wise loops in Story 5, so element-wise ships broadcast-aware from day one.

## Deferred (Post-v1)

- Complex number dtype
- Boolean dtype + boolean mask indexing
- Memory-mapped arrays
- GPU support (CUDA via cuBLAS)
- Random module (`NDArray\Random`)
- ZTS thread-safety
- Copy-on-write semantics
- Integer-array fancy indexing (v1 ships an explicit `take()` workaround)
- Pandas equivalent (separate project, depends on NumPHP)
- DLPack protocol (zero-copy tensor sharing with PyTorch / TensorFlow)
