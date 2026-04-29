# Story 8: BLAS Integration

> Part of [Epic: NumPHP](epic-numphp.md)

**Outcome:** Linear algebra ops delegate to BLAS. (LAPACK lives in Story 10.)

## Dot product

Two 1D arrays, returns a scalar:

```
A: [1, 2, 3]
B: [4, 5, 6]
result: (1×4) + (2×5) + (3×6) = 32
```

Foundation of cosine similarity:
```
cos(A, B) = dot(A, B) / (|A| × |B|)
```

## Matrix multiply

Each result element is the dot product of a row from A and a column from B.

```
A: [[1, 2],    B: [[5, 6],      Result: [[19, 22],
    [3, 4]]        [7, 8]]               [43, 50]]
```

Constraint: A's column count must equal B's row count.

## BLAS wiring

Detection happens in `config.m4` (Story 1).

dtype dispatch:
- `float64` → `cblas_dgemm` / `cblas_ddot` / `cblas_dger`
- `float32` → `cblas_sgemm` / `cblas_sdot` / `cblas_sger`
- `int32` / `int64` → no BLAS path; fall back to typed nested loops via Story 7's iterator. Performance is documented as significantly lower; users wanting fast integer matmul should cast to float.

## matmul implementation

```c
cblas_dgemm(
    CblasRowMajor,
    a_trans ? CblasTrans : CblasNoTrans,
    b_trans ? CblasTrans : CblasNoTrans,
    M,                      // A rows
    N,                      // B cols
    K,                      // A cols / B rows
    1.0,
    (double *)A_data, lda,
    (double *)B_data, ldb,
    0.0,
    (double *)result_data, ldc
);
```

## Strided BLAS — when to copy

`lda` / `ldb` / `ldc` are *leading dimensions in elements*, not byte counts. A C-contiguous `[M, K]` row-major matrix has `lda = K`. A view that slices columns (`strides[1] != itemsize`) cannot be passed directly — BLAS assumes the inner stride equals 1 element.

Decision rule:
```c
if (a->strides[a->ndim - 1] != a->itemsize) {
    a = make_contiguous_copy(a);    // inner-most stride is not unit
}
// Otherwise, lda = a->strides[a->ndim - 2] / a->itemsize
//   — works for transposed views (Fortran-contiguous reads as C with trans flag)
```

For transposed inputs, prefer setting `CblasTrans` over copying.

## API

- `NDArray::dot($a, $b)` — 1D × 1D scalar; routes to `cblas_*dot`
- `NDArray::matmul($a, $b)` — 2D × 2D; routes to `cblas_*gemm`. Higher-dim inputs treat leading dims as batch (loop in C, one gemm per batch slice)
- `NDArray::inner($a, $b)` — sum-product over the last axis; `cblas_*dot` per pair
- `NDArray::outer($a, $b)` — `[m] ⊗ [n] → [m, n]`; routes to `cblas_*ger`

Contiguity (Story 2 `flags`) is consulted before deciding to copy.
