# NaN Policy in Reductions

NumPHP follows the same rule NumPy follows: regular reductions propagate NaN, NaN-aware variants skip it. This page documents the contract precisely, including the all-NaN edge cases.

The integer dtypes (`int32`, `int64`) cannot hold NaN. The nan-variants of every reduction are aliases of the regular forms when called on integer arrays; the entries below describe behaviour on float dtypes.

---

## Regular reductions: NaN propagates

If any element in the reduction window is NaN, the result is NaN.

```php
$x = NDArray::fromArray([1.0, NAN, 3.0]);
echo $x->sum(),  "\n";      // NAN
echo $x->mean(), "\n";      // NAN
echo $x->min(),  "\n";      // NAN
echo $x->max(),  "\n";      // NAN
echo $x->std(),  "\n";      // NAN
```

```
NAN
NAN
NAN
NAN
NAN
```

This is the same behaviour as PHP's scalar `NAN` arithmetic — any operation involving NaN produces NaN.

`argmin` / `argmax` propagate NaN's effect by returning the index of the *first* NaN encountered (since NaN comparisons return false in any direction, the first NaN "wins" by virtue of being the running minimum/maximum that nothing can replace).

---

## NaN-aware variants

Eight nan-variants exist, one for each regular reduction:

| Variant | Behaviour on partial-NaN slice | All-NaN slice |
|---------|-------------------------------|---------------|
| `nansum` | sum of non-NaN elements | `0` (NaNs treated as additive identity) |
| `nanmean` | mean of non-NaN elements | `NaN` (no data) |
| `nanmin` | min of non-NaN elements | `NaN` (no data) |
| `nanmax` | max of non-NaN elements | `NaN` (no data) |
| `nanvar` | variance over non-NaN elements | `NaN` |
| `nanstd` | std over non-NaN elements | `NaN` |
| `nanargmin` | index of min, ignoring NaNs | throws `\NDArrayException` |
| `nanargmax` | index of max, ignoring NaNs | throws `\NDArrayException` |

The choice for `nansum` follows mathematical convention: an empty sum is zero. The choice for `nanmean`/`nanmin`/`nanmax` follows the "no data, no answer" convention. For `nanargmin`/`nanargmax`, "no answer" cannot be encoded as a sentinel index — every integer is a valid index — so an exception is thrown.

```php
$x = NDArray::fromArray([1.0, NAN, 3.0]);
echo $x->nansum(),  "\n";    // 4
echo $x->nanmean(), "\n";    // 2
echo $x->nanmin(),  "\n";    // 1

$allnan = NDArray::fromArray([NAN, NAN]);
echo $allnan->nansum(),  "\n";    // 0  (additive identity)
echo $allnan->nanmean(), "\n";    // NAN
try {
    $allnan->nanargmin();
} catch (\NDArrayException $e) {
    echo "threw: ", $e->getMessage(), "\n";
}
```

```
4
2
1
0
NAN
threw: nanargmin: all-NaN slice
```

---

## Per-axis behaviour

The same rule applies per-slice when an `axis` argument is given:

```php
$m = NDArray::fromArray([[1.0, 2.0, NAN], [4.0, 5.0, 6.0]]);
print_r($m->mean(1)->toArray());
print_r($m->nanmean(1)->toArray());
```

```
Array ( [0] => NAN [1] => 5 )
Array ( [0] => 1.5 [1] => 5 )
```

Row 0 has a NaN, so `mean` returns NaN for row 0; `nanmean` skips the NaN and averages over `[1.0, 2.0]`. Row 1 is fully numeric; both forms agree.

---

## Inf is not NaN

`+Inf` and `-Inf` follow IEEE arithmetic and are not skipped by the nan-variants:

```php
$x = NDArray::fromArray([1.0, INF, 3.0]);
echo $x->sum(),    "\n";    // INF
echo $x->nansum(), "\n";    // INF (Inf is not skipped)
```

If you want to filter Inf as well, do the filtering yourself before reducing.

---

## Why two reduction families

NumPHP could have shipped only nan-aware reductions and made NaN-skipping the default. We didn't, for two reasons:

1. **NaN propagation is a feature.** A NaN reaching a final result is often a signal that data validation failed earlier in the pipeline. Silent skipping turns those signals into invisible bias.
2. **NumPy compatibility.** Splitting the API the same way NumPy does makes the cheatsheet trivial and lowers the porting cost.

The default reduction is the strict, propagating one. The nan-variant is an opt-in for users who have decided NaN means "missing" rather than "broken."

---

## See also

- [Decision 10](../system.md) — NaN policy.
- [Decision 9](../system.md) — output dtype rules for reductions.
- [`NDArray` API reference](../api/ndarray.md#reductions) — every reduction method documents its NaN behaviour.
