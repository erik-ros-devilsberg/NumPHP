# NumPy ↔ NumPHP Cheat Sheet

Side-by-side translation table for the most common operations. The notes column flags places where NumPHP deliberately diverges from NumPy.

Conventions:

- Python imports `numpy as np`. PHP code assumes `numphp.so` is loaded.
- `a` and `b` are 1-D arrays of equal length unless otherwise noted; `M` is a 2-D matrix.
- "≡" means the two expressions produce identical numeric output.
- "diverges" means the behaviour is intentionally different — see notes below the table.

---

## Creation

| NumPy | NumPHP | Notes |
|-------|--------|-------|
| `np.zeros((3, 4))` | `NDArray::zeros([3, 4])` | |
| `np.ones((3, 4))` | `NDArray::ones([3, 4])` | |
| `np.full((3, 4), 7.0)` | `NDArray::full([3, 4], 7.0)` | |
| `np.eye(3)` | `NDArray::eye(3)` | Optional `m`, `k` args identical. |
| `np.arange(0, 10)` | `NDArray::arange(0, 10)` | Defaults to int64 if all args are integers. |
| `np.arange(0, 1, 0.1)` | `NDArray::arange(0, 1, 0.1)` | Inferred dtype is float64. |
| `np.array([[1,2],[3,4]])` | `NDArray::fromArray([[1,2],[3,4]])` | |
| `np.loadtxt('a.csv', delimiter=',')` | `NDArray::fromCsv('a.csv')` | NumPHP supports comma only; other delimiters deferred. |
| `np.load('a.npy')` | `NDArray::load('a.npp')` | NumPHP uses its own binary format (`NUMPHP\0\1`), not `.npy`. |

---

## Metadata

| NumPy | NumPHP | Notes |
|-------|--------|-------|
| `a.shape` | `$a->shape()` | NumPHP method; NumPy attribute. |
| `a.dtype` | `$a->dtype()` | Returns string `"float64"` etc. |
| `a.size` | `$a->size()` | Total element count. |
| `a.ndim` | `$a->ndim()` | |
| `len(a)` | `$a->shape()[0]` | **Diverges**: NumPHP's `count($a)` returns the *total* element count (same as `size()`), not the leading-axis size. Use `shape()[0]` to match NumPy's `len()`. |

---

## Indexing & slicing

| NumPy | NumPHP | Notes |
|-------|--------|-------|
| `a[0]` | `$a[0]` | View if `$a` has 2+ dims, scalar if 1-D. |
| `a[-1]` | `$a[-1]` | Negative indices count from the end in both. |
| `a[2:7]` | `$a->slice(2, 7)` | NumPHP has no `[]`-slice syntax in v1. |
| `a[2:7:2]` | `$a->slice(2, 7, 2)` | `step` arg. |
| `a[:, 1]` | `$a->transpose()->slice(1, 2)->squeeze()` | NumPHP slices only on axis 0. Multi-axis tuple slicing deferred. |
| `a[i] = x` | `$a[$i] = $x` | Scalar broadcast or NDArray of matching shape. |
| `a[mask]` | not in v1 | Boolean / fancy indexing deferred. |

---

## Shape

| NumPy | NumPHP | Notes |
|-------|--------|-------|
| `a.reshape(3, 4)` | `$a->reshape([3, 4])` | |
| `a.reshape(-1, 4)` | `$a->reshape([-1, 4])` | `-1` means "infer". |
| `a.T` or `a.transpose()` | `$a->transpose()` | Default reverse all axes. |
| `a.transpose(1, 0, 2)` | `$a->transpose([1, 0, 2])` | Explicit axis permutation. |
| `a.flatten()` | `$a->flatten()` | Always a copy in both. |
| `np.squeeze(a)` | `$a->squeeze()` | Remove all size-1 axes. |
| `np.expand_dims(a, 1)` | `$a->expandDims(1)` | |
| `np.concatenate([a, b], axis=0)` | `NDArray::concatenate([$a, $b], 0)` | |
| `np.stack([a, b])` | `NDArray::stack([$a, $b])` | |

---

## Arithmetic

| NumPy | NumPHP | Notes |
|-------|--------|-------|
| `a + b` | `$a + $b` | Operator overloaded; same broadcasting rules. |
| `a - b`, `a * b`, `a / b` | `$a - $b`, `$a * $b`, `$a / $b` | |
| `np.add(a, b)` | `NDArray::add($a, $b)` | Static method form. |
| `a + 5` | `$a + 5` | Scalar broadcast. |
| `a ** 2` | `$a->power(2)` | NumPHP has no `**` overload; method only. |

---

## Element-wise math

| NumPy | NumPHP | Notes |
|-------|--------|-------|
| `np.sqrt(a)` | `$a->sqrt()` | |
| `np.exp(a)` | `$a->exp()` | |
| `np.log(a)` | `$a->log()` | Natural log. |
| `np.log2(a)`, `np.log10(a)` | `$a->log2()`, `$a->log10()` | |
| `np.abs(a)` | `$a->abs()` | |
| `np.floor(a)`, `np.ceil(a)` | `$a->floor()`, `$a->ceil()` | |
| `np.round(a)` | `$a->round()` | **Diverges**: NumPy uses banker's rounding, NumPHP uses half-away-from-zero. See [round-half](concepts/round-half.md). |
| `np.clip(a, 0, 1)` | `$a->clip(0, 1)` | Either bound may be `null`. |

---

## Reductions

| NumPy | NumPHP | Notes |
|-------|--------|-------|
| `a.sum()` | `$a->sum()` | |
| `a.sum(axis=0)` | `$a->sum(0)` | |
| `a.sum(axis=0, keepdims=True)` | `$a->sum(0, true)` | |
| `a.mean()`, `a.min()`, `a.max()` | `$a->mean()`, `$a->min()`, `$a->max()` | |
| `a.std(ddof=1)` | `$a->std(null, false, 1)` | NumPHP signature is `std(?int $axis, bool $keepdims, int $ddof)`. |
| `a.argmin()`, `a.argmax()` | `$a->argmin()`, `$a->argmax()` | Always int64. |
| `np.nansum(a)` | `$a->nansum()` | NaN-aware variants are instance methods in NumPHP. |
| `np.nanmean(a)` | `$a->nanmean()` | |
| `np.cumsum(a)` | `$a->cumsum()` | `$axis = null` flattens. |
| `np.cumsum(a, axis=0)` | `$a->cumsum(0)` | |
| `np.cumprod(a)` | `$a->cumprod()` | **Diverges**: integer input promotes to `int64` (NumPy preserves input dtype). |
| `np.nancumsum(a)` | `$a->nancumsum()` | NaN treated as 0. |
| `np.nancumprod(a)` | `$a->nancumprod()` | NaN treated as 1. |

---

## Comparisons & where

| NumPy | NumPHP | Notes |
|-------|--------|-------|
| `a == b` | `NDArray::eq($a, $b)` | **Diverges**: PHP's `==` operator is not overloaded for NDArray. Use the static method to get an element-wise bool array. |
| `a != b` | `NDArray::ne($a, $b)` | NaN-aware: `ne(NAN, NAN)` returns `true` per IEEE 754. |
| `a < b`, `<=`, `>`, `>=` | `NDArray::lt($a, $b)`, `le`, `gt`, `ge` | All return bool NDArrays. |
| `np.where(cond, x, y)` | `NDArray::where($cond, $x, $y)` | `$cond` must be bool. Output dtype = promotion of `$x` and `$y`. |

---

## Sorting

| NumPy | NumPHP | Notes |
|-------|--------|-------|
| `np.sort(a)` | `$a->sort()` | Defaults to last axis (`-1`). |
| `np.argsort(a)` | `$a->argsort()` | |
| `np.sort(a, axis=None)` | `$a->sort(null)` | Explicit `null` flattens then sorts. |

Both NumPHP `sort` and NumPy `np.sort` are unstable for equal keys (NumPHP uses `qsort`, NumPy defaults to `quicksort`). [Decision 11](system.md) documents NumPHP's choice.

---

## Linear algebra (BLAS / LAPACK)

| NumPy | NumPHP | Notes |
|-------|--------|-------|
| `np.dot(a, b)` (1-D) | `NDArray::dot($a, $b)` | Returns float. |
| `a @ b` or `np.matmul(A, B)` | `NDArray::matmul($A, $B)` | NumPHP has no `@` operator. |
| `np.inner(a, b)` | `NDArray::inner($a, $b)` | |
| `np.outer(a, b)` | `NDArray::outer($a, $b)` | |
| `np.linalg.inv(A)` | `Linalg::inv($A)` | |
| `np.linalg.det(A)` | `Linalg::det($A)` | |
| `np.linalg.solve(A, b)` | `Linalg::solve($A, $b)` | Multi-RHS supported. |
| `np.linalg.svd(A, full_matrices=False)` | `Linalg::svd($A)` | NumPHP returns `[$U, $S, $Vt]`; v1 is thin SVD only ([decision 16](system.md)). |
| `np.linalg.eig(A)` | `Linalg::eig($A)` | NumPHP throws if eigenvalues have non-zero imaginary part — real only in v1 ([decision 15](system.md)). |
| `np.linalg.norm(a, ord=2)` | `Linalg::norm($a, 2)` | NumPHP substitutes Frobenius for matrix-2 ([decision 17](system.md)). |

---

## I/O

| NumPy | NumPHP | Notes |
|-------|--------|-------|
| `np.save('a.npy', a)` | `$a->save('a.npp')` | Different format; cannot interchange. |
| `np.load('a.npy')` | `NDArray::load('a.npp')` | |
| `np.savetxt('a.csv', a, delimiter=',')` | `$a->toCsv('a.csv')` | |
| `np.loadtxt('a.csv', delimiter=',')` | `NDArray::fromCsv('a.csv')` | |

---

## What NumPHP does NOT have in v1

- **Boolean / fancy indexing.** `a[a > 0]` and `a[[1, 3, 5]]` have no v1 equivalent.
- **Complex dtype.** `np.complex64` / `np.complex128` not supported. `Linalg::eig` throws on complex eigenvalues.
- **`np.median`, `np.percentile`, `np.quantile`.** Deferred.
- **Multi-axis reductions.** `a.sum(axis=(0, 1))` not supported — sum twice or `flatten` first.
- **Negative-step slicing.** `a[::-1]` has no equivalent in v1.
- **3D+ batched matmul.** `matmul` requires 2-D inputs.
- **Random module.** No `np.random` equivalent — use PHP's `random_*` functions or build your own.
- **In-place ops.** `np.add(a, b, out=a)` deferred; use `$a = $a + $b` (creates a new array).
- **`np.where`, `np.select`, `np.choose`.** Deferred — no boolean masks.

---

## See also

- [Concept guides](concepts/) — the "diverges" notes link here.
- [API reference](api/) — every method's exact signature, params, and behaviour.
- [`docs/system.md`](system.md) — every decision number referenced above.
