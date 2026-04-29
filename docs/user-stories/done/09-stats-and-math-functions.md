# Story 9: Statistical & Mathematical Functions

> Part of [Epic: NumPHP](epic-numphp.md)

**Outcome:** Common reductions and math ops available, axis-aware, numerically stable.

> Depends on Story 7's nd-iterator. `axis` reductions are implemented as iterators that hold the reduced axis fixed and step through the others.

## Reductions

Take an array, collapse it along an axis — analogous to `GROUP BY` in SQL.

```
A: [[1, 2, 3],
    [4, 5, 6]]

sum()        → 21         # global aggregate
sum(axis=0)  → [5, 7, 9]  # collapse rows, one value per column
sum(axis=1)  → [6, 15]    # collapse columns, one value per row
```

All reductions support an `?int $axis = null` parameter and a `bool $keepdims = false` flag (preserves the reduced axis as size 1, useful for broadcast-back).

- `sum`, `mean`, `min`, `max`, `std`, `var`
- `argmin`, `argmax` — return index of min/max along the axis (always returns `int64`)

### Numerical stability
- `mean`: pairwise sum then divide (better float roundoff than naive accumulation).
- `var` / `std`: **Welford's online algorithm** — single pass, numerically stable, no catastrophic cancellation. Naive `E[x²] - E[x]²` is rejected.
- `var(ddof: int = 0)` parameter — `ddof=1` for sample variance.

### NaN handling
- Default reductions propagate NaN: any NaN in input → NaN in output.
- nan-aware variants skip NaN: `nansum`, `nanmean`, `nanmin`, `nanmax`, `nanstd`, `nanvar`, `nanargmin`, `nanargmax`.
- Integer dtypes have no NaN; nan-variants on int dtypes are aliases of the regular forms.

## Math ops

Same shape in, same shape out. Applied element by element. dtype follows Story 1 promotion (most ops promote int → float64).

```
A: [[1, 4], [9, 16]]
sqrt(A) → [[1.0, 2.0], [3.0, 4.0]]
```

- `sqrt`, `exp`, `log`, `log2`, `log10`, `abs`, `power($base, $exp)`
- `clip(?float $min, ?float $max)` — cap values within a range; either bound nullable for one-sided clip:
  ```
  [1, 5, 10, 15, 20]
  clip(min=5, max=15) → [5, 5, 10, 15, 15]
  ```
- `floor`, `ceil`, `round(int $decimals = 0)`
- `sort(?int $axis = -1)`, `argsort(?int $axis = -1)`

## Domain errors
`sqrt(-x)` and `log(0)` follow IEEE 754 (NaN / -inf), no exception. `log` on integer dtype is promoted to f64 first; result dtype is always float.
