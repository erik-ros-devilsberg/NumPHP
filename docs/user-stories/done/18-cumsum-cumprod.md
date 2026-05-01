---
story: Cumulative reductions — cumsum and cumprod
created: 2026-05-01
---

> Part of [Epic: NumPHP](epic-numphp.md)

## Description

`np.cumsum` and `np.cumprod` are the running-total / running-product
operations. They produce an output of the same shape as the input
where each element is the sum (or product) of all elements up to and
including that position along an axis. Common in:

- **Time-series:** running totals, equity curves, drawdown calculation.
- **Probability:** building an empirical CDF from a histogram.
- **Algorithms:** prefix sums for range-query problems.

Today, the only way to do this in numphp is a per-element PHP loop —
which defeats the point of the extension. NumPy users porting code
hit `arr.cumsum()` regularly and stop.

This story adds:

1. **`NDArray::cumsum(?int $axis = null)`** — running sum.
   `axis = null` flattens then cumulates (1-D output of size
   `size()`). Specific axis cumulates along that axis, output shape
   matches input shape.
2. **`NDArray::cumprod(?int $axis = null)`** — same semantics with
   multiplication.

Both are simple inner-loop kernels. No BLAS / LAPACK involvement.
Output dtype follows the same rules as `sum` / non-existent
`cumprod`-equivalent reductions (decision 9):

| Input dtype | cumsum / cumprod output |
|---|---|
| `int32`, `int64` | `int64` (avoid overflow on accumulation) |
| `float32` | `float32` |
| `float64` | `float64` |
| `bool` (after Story 17) | `int64` |

`cumprod` on `int*` does not match NumPy precisely — NumPy preserves
the input dtype for `cumprod`. We promote to `int64` for the same
reason `sum` does (decision 9: avoid silent overflow on int32
accumulation). Document the divergence.

## Acceptance Criteria

- `NDArray::cumsum(?int $axis = null): NDArray` — instance method.
  - `axis = null`: flatten then cumulate; output is 1-D of size
    `size()`.
  - `axis = int`: cumulate along that axis; output shape == input
    shape.
  - Negative axis is allowed (per existing convention).
  - Out-of-range axis throws `\ShapeException`.
- `NDArray::cumprod(?int $axis = null): NDArray` — same.
- Output dtype follows the table above.
- NaN propagation: once a NaN appears in the running total, every
  subsequent output element is NaN (matches NumPy).
- `nancumsum` / `nancumprod` variants that skip NaN (treat as
  identity element: 0 for sum, 1 for prod). Symmetric with the
  existing `nansum` / `nanmean` / etc.
- Documentation added in `docs/api/ndarray.md` and a row each in
  `docs/cheatsheet-numpy.md`.
- New phpt tests cover both ops × all four numeric dtypes ×
  axis=null and axis=specific × NaN propagation × the int dtype
  promotion rule.

## Out of scope

- **Multi-axis cumulation.** NumPy doesn't either; not a real need.
- **Stable summation** (Kahan / Neumaier compensated sum) for very
  long axes. Default cumulative paths typically use simple fold;
  precision is bounded by `n * eps` for sum, exponential drift for
  prod. Document the limitation; revisit if accuracy becomes a
  user-visible bug.
- **In-place variants** (`$arr->cumsum_(...)`). Deferred consistent
  with the broader "no in-place ops yet" stance.
- **`cumsum` along `axis=null` on a bool array returning bool** —
  bool input promotes to int64 per the table.
