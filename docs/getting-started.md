# Getting Started with NumPHP

This guide takes you from a fresh checkout to a complete worked example: build the extension, create your first array, do some indexing and broadcasting, save it to disk, and load it back.

By the end you should be able to write PHP code that uses `NDArray` for numerical work the same way Python code uses NumPy arrays. The reference for every method appears in [`docs/api/`](api/).

---

## Prerequisites

NumPHP is a C extension. Building it requires:

- **PHP 8.2+ NTS** (non-thread-safe). ZTS builds are not supported in v1.
- **`php-dev`** (or your distro's equivalent — provides `phpize` and the PHP headers).
- **OpenBLAS** with development headers (`libopenblas-dev` on Debian/Ubuntu).
- **LAPACK** development headers (`liblapack-dev` on Debian/Ubuntu). On macOS, Accelerate provides both BLAS and LAPACK.
- **A C compiler** (`gcc` or `clang`).

Verify your PHP build:

```bash
php -v
php -r "echo PHP_ZTS ? 'ZTS' : 'NTS'; echo PHP_EOL;"
```

The second line must print `NTS`.

---

## Build

From the repo root:

```bash
phpize
./configure
make
```

Run the test suite to confirm the build:

```bash
make test
```

You should see `Tests passed: 53` (or similar — exact count varies by version) and at most one test skipped (the FFI round-trip test if the FFI extension is not loaded). If the test suite fails, do not proceed — file an issue.

To install system-wide:

```bash
sudo make install
```

To use the extension without installing, point `php` at the built `.so` directly:

```bash
php -dextension="$(pwd)/modules/numphp.so" your-script.php
```

The rest of this guide assumes you can run `php script.php` and have NumPHP loaded.

---

## Your first array

```php
<?php
$a = NDArray::zeros([2, 3]);
print_r($a->shape());      // [2, 3]
echo $a->dtype(), "\n";    // float64
echo $a->size(), "\n";     // 6
```

`NDArray::zeros` allocates a new array of the given shape filled with zeros. The default dtype is `"float64"` (a 64-bit IEEE float). The four supported dtypes are `"float32"`, `"float64"`, `"int32"`, `"int64"`. See the [dtype concept guide](concepts/dtypes.md) for the full picture.

A few more constructors:

```php
$ones = NDArray::ones([3]);                          // [1, 1, 1] as float64
$pi   = NDArray::full([2, 2], 3.14159);              // 2x2 of pi
$id   = NDArray::eye(3);                             // 3x3 identity
$seq  = NDArray::arange(0, 10);                      // [0, 1, ..., 9] as int64
$mat  = NDArray::fromArray([[1, 2, 3], [4, 5, 6]]);  // 2x3 from PHP array
```

---

## Indexing

`NDArray` implements `ArrayAccess`. Indexing along the leading axis works exactly like a PHP array:

```php
$m = NDArray::fromArray([[1, 2, 3], [4, 5, 6]]);

print_r($m[0]->toArray());   // [1, 2, 3]   (the first row, as an NDArray view)
echo $m[1][2], "\n";          // 6           (a scalar, after two index ops)
```

The result of `$m[0]` is a **view** — a new `NDArray` that shares the buffer with `$m`. Mutating it mutates `$m`:

```php
$m[0][1] = 999;
print_r($m->toArray());
// [[1, 999, 3],
//  [4, 5, 6]]
```

If you want a copy, use `clone $m[0]` or `$m[0]->reshape($m[0]->shape())`. See the [views vs copies](concepts/views-and-copies.md) concept guide for the full table of which operations view vs. copy.

For ranges, use `slice($start, $stop, $step = 1)`:

```php
$x = NDArray::arange(0, 10);
print_r($x->slice(2, 7)->toArray());   // [2, 3, 4, 5, 6]
```

`slice` only operates on axis 0 in v1. To slice along a different axis, transpose first.

---

## Broadcasting

When two arrays of different shapes meet in an arithmetic operation, NumPHP **broadcasts** them to a common shape — without copying. The rules are identical to NumPy's. The simplest case is array + scalar:

```php
$a = NDArray::fromArray([1, 2, 3]);
print_r(($a + 10)->toArray());        // [11, 12, 13]
```

Slightly more interesting — a row vector across a matrix:

```php
$m = NDArray::fromArray([[1, 2, 3], [4, 5, 6]]);     // shape (2, 3)
$v = NDArray::fromArray([10, 20, 30]);                // shape (3,)
print_r(($m + $v)->toArray());
// [[11, 22, 33],
//  [14, 25, 36]]
```

The full broadcasting rules are in [`docs/concepts/broadcasting.md`](concepts/broadcasting.md). The short version: right-align the shapes; every pair of dimensions must be equal, or one of them must be 1, or one of them must be missing.

---

## A few more useful methods

```php
$x = NDArray::arange(0, 12)->reshape([3, 4]);

echo $x->sum(),    "\n";    // 66           (full reduction)
print_r($x->sum(0)->toArray()); // [12, 15, 18, 21]  (per-column)
echo $x->mean(),   "\n";    // 5.5
echo $x->max(),    "\n";    // 11
print_r($x->transpose()->toArray());   // 4x3 (a view, no copy)
print_r($x->reshape([2, 6])->toArray()); // 2x6 (a view, x is C-contiguous)
```

For matrix operations, use the static BLAS-backed methods:

```php
$a = NDArray::fromArray([[1.0, 2.0], [3.0, 4.0]]);
$b = NDArray::fromArray([[5.0, 6.0], [7.0, 8.0]]);
print_r(NDArray::matmul($a, $b)->toArray());
// [[19, 22],
//  [43, 50]]
```

For linear algebra, use the `Linalg` class:

```php
$A = NDArray::fromArray([[3.0, 1.0], [1.0, 2.0]]);
$b = NDArray::fromArray([9.0, 8.0]);
$x = Linalg::solve($A, $b);          // Ax = b
print_r($x->toArray());              // [2, 3]

echo Linalg::det($A), "\n";          // 5
print_r(Linalg::inv($A)->toArray());
```

See the [`Linalg` API reference](api/linalg.md) for the full list (`inv`, `det`, `solve`, `svd`, `eig`, `norm`).

---

## Save and load

NumPHP has a binary file format for arrays:

```php
$a = NDArray::arange(0, 12)->reshape([3, 4]);
$a->save('/tmp/a.npp');

$b = NDArray::load('/tmp/a.npp');
print_r($b->toArray());
// Same contents as $a.
```

The format is a 16-byte header (magic + version + dtype + ndim + reserved + shape) followed by the raw little-endian buffer. The format version byte is checked first, so future versions can be detected explicitly. See [decision 20](system.md) for details.

For human-readable output, use CSV:

```php
$a->toCsv('/tmp/a.csv');
$b = NDArray::fromCsv('/tmp/a.csv');
```

CSV is locale-safe (decimal separator is always `.`) and supports `phar://` / `data://` stream wrappers via PHP's stream layer.

---

## Round-trip with PHP arrays

To convert any `NDArray` back to a nested PHP array:

```php
$a = NDArray::arange(0, 6)->reshape([2, 3]);
$as_php = $a->toArray();
// [[0, 1, 2], [3, 4, 5]]

$a2 = NDArray::fromArray($as_php);   // round-trip
```

`toArray` always copies; the result is independent of the source.

---

## What's next

- [API Reference](api/) — every public method on `NDArray`, `Linalg`, `BufferView`, plus the four exception classes.
- [Concept guides](concepts/) — dtype promotion, broadcasting, view-vs-copy, NaN policy in reductions, the round-half divergence from NumPy.
- [NumPy ↔ NumPHP cheatsheet](cheatsheet-numpy.md) — side-by-side translations of the most common operations.

---

## Common gotchas

- **`round()` is not bankers' rounding.** `NDArray::round()` rounds half-away-from-zero (matches PHP's scalar `round()`); NumPy uses banker's rounding. See [round-half concept guide](concepts/round-half.md).
- **`flatten()` is always a copy.** Use `reshape([size()])` if you want a view of a 1-D layout when the source is C-contiguous.
- **Slicing only operates on axis 0.** Use `transpose()` to bring the axis you want to the front.
- **Integer division by zero throws.** Float division by zero produces `inf`/`nan` per IEEE 754. See [decision 7](system.md).
- **`fromArray` on ragged input throws.** Every nested row must have the same length.
- **`count($arr)` returns total element count, not leading-axis size.** This is a deliberate divergence from NumPy's `len()`. Use `$arr->shape()[0]` to get the leading-axis size.
