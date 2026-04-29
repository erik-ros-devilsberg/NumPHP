# Story 10: Linear Algebra Module

> Part of [Epic: NumPHP](epic-numphp.md)

**Outcome:** `Linalg` class (static methods), backed by LAPACK.

LAPACK sits on top of BLAS — handles complex decompositions and linear system solving. BLAS is the engine, LAPACK uses it.

LAPACK linkage is established in Story 1's `config.m4`. This story does not re-do it.

## Class shape

```php
final class Linalg {
    public static function inv(NDArray $a): NDArray;
    public static function det(NDArray $a): float;
    public static function svd(NDArray $a): array;        // [U, S, Vt]
    public static function eig(NDArray $a): array;        // [values, vectors]
    public static function solve(NDArray $a, NDArray $b): NDArray;
    public static function norm(NDArray $a, int|string $ord = 2, ?int $axis = null): float|NDArray;
}
```

A static class is preferred over a `\NDArray\Linalg` namespace — PHP namespace registration in C extensions is awkward, and the static class reads identically at the call site (`Linalg::inv($a)`).

## Operations and LAPACK calls

All ops dispatch by dtype: `f64` → `d*` routine, `f32` → `s*` routine. Integer inputs are promoted to `f64` first.

| Op       | What it does                                | LAPACK (f64)    |
|----------|---------------------------------------------|-----------------|
| `inv`    | Matrix inverse (`A × B = I` ⇒ `B = A⁻¹`)   | `dgetrf` + `dgetri` |
| `det`    | Determinant; `det = 0` ⇒ singular           | `dgetrf` (product of U diag, sign from pivots) |
| `svd`    | `A = U Σ Vᵀ`; recommendation systems, PCA   | `dgesdd`        |
| `eig`    | Eigenvalues and eigenvectors                | `dgeev`         |
| `solve`  | `Ax = b`; more stable than `inv(A) @ b`     | `dgesv`         |
| `norm`   | Vector / matrix size                        | `dlange`, `dnrm2` |

## Contiguity & layout
LAPACK is column-major (Fortran). Inputs must be either:
- F-contiguous — pass directly, set `lda = shape[0]`
- C-contiguous — copy to F-contiguous scratch buffer, or use the `LAPACKE_*_row_major` wrappers if available

Use the `LAPACKE_*` row-major wrappers when present; fall back to manual transpose otherwise.

## Errors
- Singular matrix in `inv` / `solve` → `\NDArrayException` with the LAPACK info code.
- Non-square input where square is required → `\ShapeException`.
- Dimension mismatch in `solve` → `\ShapeException`.
