# NumPHP

A native PHP C extension providing n-dimensional array primitives, vectorised operations, and BLAS/LAPACK bindings — making PHP viable for numerical computing.

```php
$a = NDArray::fromArray([[1.0, 2.0], [3.0, 4.0]]);
$b = NDArray::fromArray([[5.0, 6.0], [7.0, 8.0]]);
print_r(NDArray::matmul($a, $b)->toArray());
// [[19, 22],
//  [43, 50]]
```

## Status

Pre-1.0. Eleven of thirteen user stories shipped (creation, indexing, broadcasting, shape, BLAS, stats, linalg, file I/O, FFI bridge). Documentation pass and PECL packaging in progress.

## Features

- Four dtypes: `float32`, `float64`, `int32`, `int64`.
- N-dimensional arrays up to rank 16, refcounted buffer with view semantics.
- Operator overloading for `+`, `-`, `*`, `/` with broadcasting.
- Reductions (`sum`, `mean`, `min`, `max`, `var`, `std`, `argmin`, `argmax`) plus NaN-aware variants.
- BLAS-backed `matmul`, `dot`, `inner`, `outer`.
- LAPACK-backed `Linalg::inv`, `det`, `solve`, `svd`, `eig`, `norm`.
- Save/load to a versioned binary format; CSV reader and writer.
- `BufferView` for zero-copy handoff to PHP FFI consumers.

## Requirements

- PHP 8.2+ NTS (non-thread-safe).
- OpenBLAS + LAPACK headers (`libopenblas-dev` `liblapack-dev` on Debian/Ubuntu; Accelerate on macOS).
- A C compiler and `phpize`.

## Build

```bash
phpize
./configure
make
make test
```

To install system-wide:

```bash
sudo make install
echo "extension=numphp.so" | sudo tee /etc/php/8.4/mods-available/numphp.ini
sudo phpenmod numphp
```

To use without installing:

```bash
php -dextension="$(pwd)/modules/numphp.so" your-script.php
```

## Documentation

- [Getting started](docs/getting-started.md) — install → first array → indexing → broadcasting → save/load.
- [API reference](docs/api/) — every public method on `NDArray`, `Linalg`, `BufferView`, plus the four exception classes.
- [Concept guides](docs/concepts/) — dtype promotion, broadcasting, view-vs-copy, NaN policy, the round-half divergence from NumPy.
- [NumPy ↔ NumPHP cheat sheet](docs/cheatsheet-numpy.md) — side-by-side translations.
- [System decisions](docs/system.md) — cross-cutting architectural choices, locked.

## Hello array

```php
<?php
$a = NDArray::arange(0, 12)->reshape([3, 4]);

echo $a->sum(),         "\n";   // 66
echo $a->mean(),        "\n";   // 5.5
print_r($a->sum(0)->toArray()); // [12, 15, 18, 21]
print_r($a->transpose()->shape()); // [4, 3]
```

## License

BSD 3-Clause — same as NumPy, SciPy, and the broader numerical-Python ecosystem. See [`LICENSE`](LICENSE).
