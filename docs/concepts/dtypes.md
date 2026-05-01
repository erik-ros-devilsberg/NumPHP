# Dtype Promotion

NumPHP supports five dtypes in v1: `"float32"`, `"float64"`, `"int32"`, `"int64"`, and `"bool"`. Complex and float16 are deferred. Every method that accepts a `dtype` argument takes one of these five strings; every NDArray reports its dtype via `$arr->dtype()`.

`bool` is stored as 1 byte per element with canonical `0`/`1` values: writes are canonicalised (`(value != 0) ? 1 : 0`); reads accept any non-zero byte as true. See [decision 32](../system.md).

When two arrays of different dtypes meet — in `add`, `subtract`, `multiply`, `divide`, `concatenate`, `stack`, and so on — the output dtype is determined by a fixed promotion table. This page documents that table, the rule behind it, and the places where it does **not** apply.

---

## The rule

The output dtype is the smallest dtype that can hold both inputs' values without loss. In practice:

- `int + int → wider int`
- `int + float → float of at least equal precision`
- `mixed int/float widths → float64`

Equivalently: NumPHP follows NumPy's promotion rules for these four types.

---

## The full table

| op1 \ op2 | f32 | f64 | i32 | i64 | bool |
|-----------|-----|-----|-----|-----|------|
| **f32**   | f32 | f64 | f32 | f64 | f32  |
| **f64**   | f64 | f64 | f64 | f64 | f64  |
| **i32**   | f32 | f64 | i32 | i64 | i32  |
| **i64**   | f64 | f64 | i64 | i64 | i64  |
| **bool**  | f32 | f64 | i32 | i64 | bool |

Read row-then-column. The table is symmetric (`f32 + i64 == i64 + f32 == f64`).

`bool` acts as the bottom of the hierarchy: pairing bool with anything else preserves the other dtype. Only `bool + bool = bool` — and that case is effectively logical OR, since the bool write path canonicalises any non-zero result back to `1` ([decision 32](../system.md)).

---

## Worked examples

```php
$a = NDArray::ones([3], "int32");
$b = NDArray::ones([3], "int64");
echo NDArray::add($a, $b)->dtype(), "\n";    // int64
```

```
int64
```

```php
$a = NDArray::ones([3], "int32");
$b = NDArray::ones([3], "float32");
echo NDArray::add($a, $b)->dtype(), "\n";    // float32 (per the table)
```

```
float32
```

```php
$a = NDArray::ones([3], "int64");
$b = NDArray::ones([3], "float32");
echo NDArray::add($a, $b)->dtype(), "\n";    // float64 (mixed widths)
```

```
float64
```

---

## When the rule does not apply: BLAS

Decisions 8 (Story 8) and 12-14 (Story 10) carve out a wider rule for BLAS / LAPACK paths:

- **`dot`, `matmul`, `inner`, `outer`** — pure float32 inputs use the `s*` (single precision) path. Anything else, including any int input, promotes to `float64` and uses the `d*` path. There is no integer matmul in v1.
- **`Linalg::*`** — same rule. Int input → `float64`. Pure `f32` → `s*` LAPACK; otherwise `d*`.

This is wider than the elementwise table because BLAS has no integer routines. If you need to keep int dtype through a BLAS-like operation, do the dtype management yourself before/after.

---

## Reductions: a separate table

Reduction ops (`sum`, `mean`, `var`, `std`, `argmin`, `argmax`, `cumsum`, `cumprod`) have their own dtype rules — these are not in the binary-op promotion table.

| Op | bool input | int input | f32 input | f64 input |
|----|-----------|-----------|-----------|-----------|
| `sum` | int64 | int64 | f32 | f64 |
| `mean`, `var`, `std` | f64 | f64 | f32 | f64 |
| `min`, `max` | bool | preserve | preserve | preserve |
| `argmin`, `argmax` | int64 | int64 | int64 | int64 |
| `cumsum`, `cumprod` | int64 | int64 | f32 | f64 |

`sum` of int promotes to `int64` to avoid silent overflow when accumulating an `int32` input. `argmin` / `argmax` always return `int64` regardless of input dtype. See [decision 9](../system.md) for the full reasoning.

`cumsum` / `cumprod` follow the same int-promotion logic. **`cumprod` on integer input is a deliberate divergence from NumPy** — NumPy preserves the input dtype, NumPHP returns `int64` for consistency with `cumsum` and `sum`. See [decision 31](../system.md).

`bool` flows through reductions as if it were below `int32` — `sum` counts the trues as `int64`, `mean` returns the proportion of trues as `f64`, `min` / `max` preserve bool. See [decision 34](../system.md).

---

## What NumPHP does NOT promote

- **Scalars passed alongside an NDArray** are wrapped in a 0-D temporary and follow the same table. `$arr + 1.5` where `$arr` is `int32` produces `float64` (mixed widths) — same as `$arr + NDArray::full([], 1.5)`.
- **`offsetSet` does not promote.** `$arr[0] = $value` requires `$value` to be representable in `$arr`'s dtype. There is no "store an int into a float array and silently widen" — the assignment converts `$value` to `$arr`'s dtype as best it can; out-of-range values throw `\DTypeException` for integer destinations and become `inf`/`nan` for floats.

---

## Why these five dtypes

The v1 dtypes are chosen to:

- Cover the dominant cases: 64-bit double for science, 32-bit float for performance / memory savings, two integer widths for indexing and discrete data, and `bool` for masks and comparison results.
- Avoid the half-supported zoo (uint8, uint16, complex64, etc.). A small surface is easier to reason about and test.
- Match what BLAS / LAPACK actually provides without bridging code (bool inputs to BLAS promote to f64 via the existing rule).

Complex is deferred to v2 — it requires complex-aware reductions, complex eigenvalues from `Linalg::eig`, and a dtype-aware printer. Float16 has no LAPACK support; deferred indefinitely.

---

## See also

- [Decision 2](../system.md) — dtype scope.
- [Decision 3](../system.md) — the original 4×4 promotion table.
- [Decision 9](../system.md) — output dtype rules for reductions.
- [Decision 31](../system.md) — `cumprod` on int promotes to int64.
- [Decision 32](../system.md) — bool storage layout (canonical 0/1).
- [Decision 34](../system.md) — bool through reductions.
- [`NDArray` API reference](../api/ndarray.md) — every method's output dtype is documented.
