# Linalg

The `Linalg` class is a holder for static linear-algebra routines. Every method is `public static` and operates on `NDArray` inputs of float dtype (`float32` or `float64`). Integer inputs are promoted to `float64` internally.

All routines call into LAPACK directly (decision 12 in [`docs/system.md`](../system.md)). On macOS the symbols come from Accelerate; on Linux from the system LAPACK / OpenBLAS LAPACK. The `s*` (single precision) path is taken when both inputs are pure `float32`; otherwise everything promotes to `float64` and uses `d*`.

Every input is materialised to a column-major (F-contiguous) scratch buffer before the LAPACK call (decision 14). The result is transpose-copied back into row-major NDArray form. Strided LAPACK ("zero-copy" handling of transposed views) is deferred.

---

## Linalg::inv(): NDArray

Matrix inverse via LU factorisation.

**Signature:** `public static function inv(NDArray $a): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$a` | `NDArray` | required | Square 2-D matrix. |

**Returns:** `NDArray` of same shape and dtype, the inverse of `$a`.

**Throws:** `\ShapeException` if `$a` is not square 2-D. `\NDArrayException` if `$a` is singular (LAPACK reports a zero pivot during LU).

**LAPACK routines:** `dgetrf` + `dgetri` (or `sgetrf` + `sgetri`).

**Example:**

```php
$a = NDArray::fromArray([[4.0, 7.0], [2.0, 6.0]]);
print_r(Linalg::inv($a)->toArray());
```

```
Array ( [0] => Array ( [0] => 0.6 [1] => -0.7 )
        [1] => Array ( [0] => -0.2 [1] => 0.4 ) )
```

---

## Linalg::det(): float

Determinant.

**Signature:** `public static function det(NDArray $a): float`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$a` | `NDArray` | required | Square 2-D matrix. |

**Returns:** `float` — `det(a)`.

**Throws:** `\ShapeException` if `$a` is not square 2-D.

**Notes:** Returns `0.0` for singular matrices (no exception) — distinguishes itself from `inv()` which does throw. If you need the sign and log-magnitude of the determinant for very large matrices, `slogdet` would be the addition; deferred to a future story.

**LAPACK:** `dgetrf` (computes the determinant from the diagonal of the LU factorisation, with a sign correction from row pivots).

**Example:**

```php
$a = NDArray::fromArray([[4.0, 7.0], [2.0, 6.0]]);
echo Linalg::det($a), "\n";
```

```
10
```

---

## Linalg::solve(): NDArray

Solve the linear system `A · x = b`.

**Signature:** `public static function solve(NDArray $a, NDArray $b): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$a` | `NDArray` | required | Square 2-D matrix `(n, n)`. |
| `$b` | `NDArray` | required | Right-hand side. Either a 1-D vector of length `n`, or a 2-D matrix `(n, k)` for multi-RHS solve. |

**Returns:** `NDArray` of the same shape as `$b` — `x` such that `a @ x == b`.

**Throws:** `\ShapeException` if shapes are incompatible. `\NDArrayException` if `$a` is singular.

**LAPACK:** `dgesv` / `sgesv`.

**Example:**

```php
$a = NDArray::fromArray([[3.0, 1.0], [1.0, 2.0]]);
$b = NDArray::fromArray([9.0, 8.0]);
print_r(Linalg::solve($a, $b)->toArray());
```

```
Array ( [0] => 2 [1] => 3 )
```

---

## Linalg::svd(): array

Thin singular-value decomposition: `A = U · diag(S) · Vt` (decision 16).

**Signature:** `public static function svd(NDArray $a): array`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$a` | `NDArray` | required | 2-D matrix `(m, n)`. |

**Returns:** `array{U: NDArray, S: NDArray, Vt: NDArray}` (PHP indexed array `[$U, $S, $Vt]`).

| Output | Shape |
|--------|-------|
| `U` | `(m, k)` |
| `S` | `(k,)` (singular values, descending) |
| `Vt` | `(k, n)` |

where `k = min(m, n)`.

**Throws:** `\ShapeException` if `$a` is not 2-D. `\NDArrayException` on LAPACK convergence failure.

**Notes:**

- v1 always uses `JOBZ='S'` (thin SVD). Full SVD (`U` of shape `(m,m)`, `Vt` of shape `(n,n)`) is deferred.
- LAPACK: `dgesdd` / `sgesdd`.

---

## Linalg::eig(): array

Eigenvalues and right eigenvectors of a real square matrix.

**Signature:** `public static function eig(NDArray $a): array`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$a` | `NDArray` | required | Square 2-D matrix `(n, n)`. |

**Returns:** `[NDArray $w, NDArray $v]` — `$w` is shape `(n,)` real eigenvalues; `$v` is shape `(n, n)` right eigenvectors (column `i` is the eigenvector for `$w[i]`).

**Throws:**

- `\ShapeException` if `$a` is not square 2-D.
- `\NDArrayException` if any returned eigenvalue has a non-zero imaginary part (decision 15 — v1 supports real eigenvalues only). The exception message names the offending index and the imaginary magnitude. Workaround: ensure `$a` is symmetric (`A == A^T`) until complex dtype lands in v2.

**LAPACK:** `dgeev` / `sgeev`.

---

## Linalg::norm(): float|NDArray

Vector or matrix norm.

**Signature:** `public static function norm(NDArray $a, mixed $ord = 2, ?int $axis = null): float|NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$a` | `NDArray` | required | 1-D or 2-D input. |
| `$ord` | `int\|float\|"fro"` | `2` | Norm type. See table below. |
| `$axis` | `?int` | `null` | If given, treat as a stack of vectors and reduce along that axis. |

**Norm types:**

| `$a` shape | `$ord` | Returns |
|------------|--------|---------|
| 1-D | `2` (default) | Euclidean / `sqrt(sum(x*x))` |
| 1-D | `1` | sum of absolute values |
| 1-D | `inf` (PHP `INF`) | max of absolute values |
| 2-D | `2` (default) | **Frobenius** norm — see decision 17 |
| 2-D | `"fro"` | Frobenius norm |
| 2-D | `1` | max of column sums of abs |
| 2-D | `inf` | max of row sums of abs |

**Returns:** `float` for full reduction, `NDArray` if `$axis` is given.

**Notes:**

- v1 substitutes Frobenius for matrix-2 (spectral) norm. The true spectral norm requires SVD; defer until a user needs it (decision 17).

**Example:**

```php
echo Linalg::norm(NDArray::fromArray([3.0, 4.0])), "\n";    // 1-D, ord=2
```

```
5
```

---

## See also

- [`NDArray`](ndarray.md) — the array class consumed by every Linalg routine.
- [`docs/system.md`](../system.md) — decisions 12-17 capture the design choices behind this module.
