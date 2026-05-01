# NDArray

The `NDArray` class is the n-dimensional array primitive of NumPHP. It implements `ArrayAccess` and `Countable`. Instances are immutable in shape but mutable in element values.

NDArray supports four dtypes: `"float32"`, `"float64"`, `"int32"`, `"int64"`. Maximum dimensionality is 16. Most operations promote dtypes following the [promotion table](../concepts/dtypes.md). Many shape operations return views (zero-copy aliases over the same buffer); see [views vs copies](../concepts/views-and-copies.md).

The arithmetic operators `+`, `-`, `*`, `/` are overloaded — `$a + $b` is equivalent to `NDArray::add($a, $b)` and supports broadcasting plus scalar operands.

---

## Static factories

### NDArray::zeros(): NDArray

Create a new array of the given shape filled with zeros.

**Signature:** `public static function zeros(array $shape, string $dtype = "float64"): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$shape` | `array` | required | Array of positive integers giving the size of each dimension. |
| `$dtype` | `string` | `"float64"` | One of `"float32"`, `"float64"`, `"int32"`, `"int64"`. |

**Returns:** `NDArray` — newly allocated, C-contiguous, all elements zero.

**Throws:** `\ShapeException` if `$shape` contains non-positive integers or has more than 16 dimensions. `\DTypeException` if `$dtype` is unrecognised.

**Example:**

```php
<?php
$a = NDArray::zeros([2, 3]);
print_r($a->shape());
echo $a->dtype(), "\n";
echo $a->size(), "\n";
```

```
Array ( [0] => 2 [1] => 3 )
float64
6
```

---

### NDArray::ones(): NDArray

Create a new array of the given shape filled with ones.

**Signature:** `public static function ones(array $shape, string $dtype = "float64"): NDArray`

**Parameters:** identical to `zeros()`.

**Returns:** `NDArray` — newly allocated, C-contiguous, all elements one.

**Throws:** as `zeros()`.

**Example:**

```php
$a = NDArray::ones([3], "int32");
print_r($a->toArray());
```

```
Array ( [0] => 1 [1] => 1 [2] => 1 )
```

---

### NDArray::full(): NDArray

Create a new array of the given shape filled with a single value.

**Signature:** `public static function full(array $shape, mixed $value, string $dtype = "float64"): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$shape` | `array` | required | Shape array. |
| `$value` | `int\|float` | required | Fill value. Coerced to `$dtype`. |
| `$dtype` | `string` | `"float64"` | Element dtype. |

**Returns:** `NDArray` — every element equals `$value` cast to `$dtype`.

**Throws:** `\ShapeException` for invalid shape; `\DTypeException` for an unknown `$dtype` string. Non-numeric `$value` is coerced to a number using PHP's standard cast (e.g. `"hello"` → `0`); `NaN` cast to an integer dtype is undefined behaviour at the C level — the doc recommendation is to validate `$value` in PHP before calling `full()`.

**Example:**

```php
$pi = NDArray::full([2, 2], 3.14);
print_r($pi->toArray());
```

```
Array ( [0] => Array ( [0] => 3.14 [1] => 3.14 ) [1] => Array ( [0] => 3.14 [1] => 3.14 ) )
```

---

### NDArray::eye(): NDArray

Identity-style matrix: ones on a chosen diagonal, zeros elsewhere.

**Signature:** `public static function eye(int $n, ?int $m = null, int $k = 0, string $dtype = "float64"): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$n` | `int` | required | Number of rows. |
| `$m` | `?int` | `null` | Number of columns. `null` means same as `$n`. |
| `$k` | `int` | `0` | Diagonal offset: `0` is the main diagonal, positive offsets the upper diagonals, negative the lower. |
| `$dtype` | `string` | `"float64"` | Element dtype. |

**Returns:** `NDArray` of shape `[$n, $m]`.

**Throws:** `\ShapeException` if `$n` or `$m` is non-positive.

**Example:**

```php
$I = NDArray::eye(3);
print_r($I->toArray());
```

```
Array ( [0] => Array ( [0] => 1 [1] => 0 [2] => 0 )
        [1] => Array ( [0] => 0 [1] => 1 [2] => 0 )
        [2] => Array ( [0] => 0 [1] => 0 [2] => 1 ) )
```

---

### NDArray::arange(): NDArray

1-D array of evenly spaced values within `[start, stop)`.

**Signature:** `public static function arange(int|float $start, int|float $stop, int|float $step = 1, ?string $dtype = null): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$start` | `int\|float` | required | First value (inclusive). |
| `$stop` | `int\|float` | required | End value (exclusive). |
| `$step` | `int\|float` | `1` | Spacing. May be negative. |
| `$dtype` | `?string` | `null` | If `null`, inferred: int if all three args are integers and `$step` divides evenly, else `float64`. |

**Returns:** `NDArray` of shape `[ceil((stop - start) / step)]`.

**Throws:** `\NDArrayException` if `$step == 0`. `\ShapeException` if the resulting length would be zero or negative.

**Example:**

```php
$x = NDArray::arange(0, 5);
print_r($x->toArray());
echo $x->dtype(), "\n";
```

```
Array ( [0] => 0 [1] => 1 [2] => 2 [3] => 3 [4] => 4 )
int64
```

---

### NDArray::fromArray(): NDArray

Build an `NDArray` from a (possibly nested) PHP array. Shape is inferred from the nesting depth.

**Signature:** `public static function fromArray(array $data, ?string $dtype = null): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$data` | `array` | required | Rectangular PHP array. Nesting depth determines `ndim`. |
| `$dtype` | `?string` | `null` | If `null`, inferred: `int64` if every leaf is integer, else `float64`. |

**Returns:** `NDArray` with shape matching the nested array's structure.

**Throws:** `\ShapeException` if the input is ragged (e.g. `[[1,2],[3]]`) or has mixed leaf-and-branch siblings at the same depth. `\DTypeException` for an unknown `$dtype` string. Non-numeric leaf values are coerced via PHP's standard cast (`"abc"` → `0`); validate inputs in PHP if you need to reject non-numeric values.

**Example:**

```php
$m = NDArray::fromArray([[1, 2, 3], [4, 5, 6]]);
print_r($m->shape());
echo $m->dtype(), "\n";
```

```
Array ( [0] => 2 [1] => 3 )
int64
```

---

### NDArray::fromCsv(): NDArray

Parse a CSV file into a 2-D array. Comma-separated, RFC-4180-ish.

**Signature:** `public static function fromCsv(string $path, string $dtype = "float64", bool $header = false): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$path` | `string` | required | File path. Honours PHP's stream wrappers (`phar://`, `data://`, …). |
| `$dtype` | `string` | `"float64"` | All cells parsed to this dtype. |
| `$header` | `bool` | `false` | If `true`, the first line is skipped. Header values are not retained — use a separate parse if you need them. |

**Returns:** `NDArray` of shape `[rows, cols]`.

**Throws:** `\NDArrayException` if the file cannot be opened, if rows have inconsistent column counts, or if a cell cannot be parsed as `$dtype`.

**Notes:**

- Float parsing uses `zend_strtod` — locale-independent (`.` is always the decimal separator).
- Other delimiters and per-column dtype detection are deferred post-v1.

**Example:**

```php
file_put_contents('/tmp/m.csv', "1,2,3\n4,5,6\n");
$m = NDArray::fromCsv('/tmp/m.csv');
print_r($m->shape());
```

```
Array ( [0] => 2 [1] => 3 )
```

---

### NDArray::load(): NDArray

Load an array from the NumPHP binary format produced by [`save()`](#ndarraysave-void).

**Signature:** `public static function load(string $path): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$path` | `string` | required | File path. |

**Returns:** `NDArray` reconstructed from the file (dtype, shape, and contents preserved).

**Throws:** `\NDArrayException` if the file cannot be opened, the magic bytes do not match `NUMPHP\0`, or the format version is newer than this build understands. See [decision 20](../system.md) for the binary format.

**Example:**

```php
$a = NDArray::arange(0, 6)->reshape([2, 3]);
$a->save('/tmp/a.npp');
$b = NDArray::load('/tmp/a.npp');
print_r($b->toArray());
```

```
Array ( [0] => Array ( [0] => 0 [1] => 1 [2] => 2 )
        [1] => Array ( [0] => 3 [1] => 4 [2] => 5 ) )
```

---

## Metadata

### NDArray::shape(): array

Return the dimensions as a PHP array.

**Signature:** `public function shape(): array`

**Returns:** `array` of non-negative integers, one per dimension.

**Example:**

```php
$a = NDArray::zeros([2, 3, 4]);
print_r($a->shape());
```

```
Array ( [0] => 2 [1] => 3 [2] => 4 )
```

---

### NDArray::dtype(): string

Return the element dtype name.

**Signature:** `public function dtype(): string`

**Returns:** `string` — one of `"float32"`, `"float64"`, `"int32"`, `"int64"`.

**Example:**

```php
echo NDArray::ones([2], "int32")->dtype(), "\n";
```

```
int32
```

---

### NDArray::size(): int

Return the total number of elements (product of `shape()`).

**Signature:** `public function size(): int`

**Returns:** `int`.

**Example:**

```php
echo NDArray::zeros([2, 3, 4])->size(), "\n";
```

```
24
```

---

### NDArray::ndim(): int

Return the number of dimensions (length of `shape()`).

**Signature:** `public function ndim(): int`

**Returns:** `int`. Always between 0 and 16 inclusive.

**Example:**

```php
echo NDArray::zeros([2, 3])->ndim(), "\n";
```

```
2
```

---

## Indexing (ArrayAccess + Countable)

`NDArray` implements `ArrayAccess` so `$a[$i]` and `$a[$i] = $value` work. It implements `Countable` so `count($a)` returns the **total** element count (same as `size()`). This diverges from NumPy's `len()`, which returns the leading-axis size — see the [`count()` entry](#ndarraycount-int) below. See [indexing concept guide](../concepts/views-and-copies.md) for view-vs-copy rules.

### NDArray::offsetGet(): mixed

Return either a scalar (for full indexing of a 1-D array) or a sub-view (for partial indexing of an n-D array).

**Signature:** `public function offsetGet(mixed $offset): mixed`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$offset` | `int` | required | Index along the leading axis. Negative indices count from the end. |

**Returns:**
- `int\|float` if the array is 1-D — the scalar at that position.
- `NDArray` view (n-1)-dimensional if the array has 2+ dimensions.

**Throws:** `\IndexException` if `$offset` is out of range (after negative-index normalisation).

**Example:**

```php
$m = NDArray::fromArray([[1, 2, 3], [4, 5, 6]]);
print_r($m[0]->toArray());   // first row
echo $m[1][2], "\n";          // scalar
```

```
Array ( [0] => 1 [1] => 2 [2] => 3 )
6
```

---

### NDArray::offsetSet(): void

Assign into the array along the leading axis.

**Signature:** `public function offsetSet(mixed $offset, mixed $value): void`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$offset` | `int` | required | Index along the leading axis. |
| `$value` | `int\|float\|NDArray` | required | Scalar (broadcast across the slot) or NDArray (shape must match). |

**Returns:** `void`.

**Throws:** `\IndexException` for out-of-range; `\ShapeException` if `$value` is an NDArray of incompatible shape.

**Example:**

```php
$m = NDArray::zeros([2, 3]);
$m[0] = NDArray::fromArray([1, 2, 3]);
$m[1] = 99;                                 // scalar broadcast
print_r($m->toArray());
```

```
Array ( [0] => Array ( [0] => 1 [1] => 2 [2] => 3 )
        [1] => Array ( [0] => 99 [1] => 99 [2] => 99 ) )
```

---

### NDArray::offsetExists(): bool

Return `true` if `$offset` is in range along the leading axis (after negative-index normalisation).

**Signature:** `public function offsetExists(mixed $offset): bool`

---

### NDArray::offsetUnset(): void

Always throws — element removal is not supported.

**Signature:** `public function offsetUnset(mixed $offset): void`

**Throws:** `\NDArrayException` always. Shape is fixed once allocated.

---

### NDArray::count(): int

Return the total number of elements (same as `size()`). Implements `Countable`, so `count($arr)` and `$arr->count()` agree.

**Signature:** `public function count(): int`

**Notes:** This **diverges from NumPy**. NumPy's `len(a)` returns the size of the leading axis (`shape[0]`); NumPHP's `count()` returns the total element count. The PHP `Countable` contract says "the count of items" — NumPHP interprets this as "all elements" rather than "leading axis," matching `size()`. To get the leading-axis size, use `$arr->shape()[0]`.

**Example:**

```php
echo count(NDArray::zeros([4, 7])), "\n";   // 28 — total, not 4
echo NDArray::zeros([4, 7])->shape()[0], "\n";   // 4 — leading-axis size
```

```
28
4
```

---

## Slicing

### NDArray::slice(): NDArray

Return a view that selects a range along axis 0.

**Signature:** `public function slice(int $start, int $stop, int $step = 1): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$start` | `int` | required | First index (inclusive). Negative counts from the end. |
| `$stop` | `int` | required | Last index (exclusive). Negative counts from the end. |
| `$step` | `int` | `1` | Stride. Must be positive in v1. |

**Returns:** `NDArray` view sharing the source buffer.

**Throws:** `\IndexException` for invalid bounds; `\NDArrayException` if `$step <= 0` (negative step deferred to a future version).

**Notes:**

- For multi-axis slicing, chain `slice()` after `transpose()` so the desired axis is leading.
- The result is a view — mutations via `offsetSet` are visible in the source.

**Example:**

```php
$a = NDArray::arange(0, 10);
print_r($a->slice(2, 7)->toArray());
```

```
Array ( [0] => 2 [1] => 3 [2] => 4 [3] => 5 [4] => 6 )
```

---

## Shape (instance methods)

### NDArray::reshape(): NDArray

Return an array with the same data and a new shape. View when the source is C-contiguous; copy otherwise.

**Signature:** `public function reshape(array $shape): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$shape` | `array` | required | New shape. Exactly one dimension may be `-1`, meaning "infer from the total size." |

**Returns:** `NDArray` — view if source is C-contiguous, otherwise a fresh contiguous copy.

**Throws:** `\ShapeException` if the new shape's product does not equal `size()`, or if more than one dimension is `-1`.

**Example:**

```php
$x = NDArray::arange(0, 12)->reshape([3, -1]);
print_r($x->shape());
```

```
Array ( [0] => 3 [1] => 4 )
```

---

### NDArray::transpose(): NDArray

Permute the axes. Returns a view (no copy).

**Signature:** `public function transpose(?array $axes = null): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$axes` | `?array` | `null` | Permutation of `[0..ndim-1]`. `null` means reverse all axes. |

**Returns:** `NDArray` view with rearranged strides. Shape is permuted; data is unchanged.

**Throws:** `\ShapeException` if `$axes` is not a valid permutation.

**Example:**

```php
$m = NDArray::arange(0, 6)->reshape([2, 3]);
print_r($m->transpose()->toArray());
```

```
Array ( [0] => Array ( [0] => 0 [1] => 3 )
        [1] => Array ( [0] => 1 [1] => 4 )
        [2] => Array ( [0] => 2 [1] => 5 ) )
```

---

### NDArray::flatten(): NDArray

Return a 1-D copy of all elements in row-major order.

**Signature:** `public function flatten(): NDArray`

**Returns:** Always a fresh, C-contiguous, 1-D copy. (Use `reshape([size()])` if you want a view when the source is already contiguous.)

**Example:**

```php
$m = NDArray::arange(0, 6)->reshape([2, 3]);
print_r($m->flatten()->toArray());
```

```
Array ( [0] => 0 [1] => 1 [2] => 2 [3] => 3 [4] => 4 [5] => 5 )
```

---

### NDArray::squeeze(): NDArray

Remove dimensions of size 1. Returns a view.

**Signature:** `public function squeeze(?int $axis = null): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$axis` | `?int` | `null` | If specified, only that axis is removed (it must have size 1). If `null`, all size-1 dimensions are removed. |

**Returns:** `NDArray` view.

**Throws:** `\ShapeException` if `$axis` is given and that axis has size != 1.

**Example:**

```php
$x = NDArray::zeros([1, 3, 1]);
echo $x->squeeze()->ndim(), "\n";
```

```
1
```

---

### NDArray::expandDims(): NDArray

Insert a size-1 axis at the given position. Returns a view.

**Signature:** `public function expandDims(int $axis): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$axis` | `int` | required | Position of the new axis in the result shape. Negative indices count from the end. |

**Returns:** `NDArray` view with one extra dimension of size 1.

**Throws:** `\ShapeException` if `$axis` is out of range or if the result would exceed 16 dimensions.

**Example:**

```php
$x = NDArray::arange(0, 3);
print_r($x->expandDims(0)->shape());
```

```
Array ( [0] => 1 [1] => 3 )
```

---

## Shape (static methods)

### NDArray::concatenate(): NDArray

Join arrays along an existing axis.

**Signature:** `public static function concatenate(array $arrays, int $axis = 0): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$arrays` | `array` | required | Up to 64 NDArray inputs. All must have the same `ndim`. All shapes must agree on every axis except `$axis`. |
| `$axis` | `int` | `0` | Axis to join along. |

**Returns:** `NDArray` — fresh contiguous copy. Output dtype is the promotion of all input dtypes.

**Throws:** `\ShapeException` for shape mismatches or empty input. `\NDArrayException` if more than 64 inputs are given.

**Example:**

```php
$a = NDArray::fromArray([[1, 2], [3, 4]]);
$b = NDArray::fromArray([[5, 6]]);
print_r(NDArray::concatenate([$a, $b], 0)->toArray());
```

```
Array ( [0] => Array ( [0] => 1 [1] => 2 )
        [1] => Array ( [0] => 3 [1] => 4 )
        [2] => Array ( [0] => 5 [1] => 6 ) )
```

---

### NDArray::stack(): NDArray

Join arrays along a *new* axis. All inputs must have identical shape.

**Signature:** `public static function stack(array $arrays, int $axis = 0): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$arrays` | `array` | required | Up to 64 NDArrays of identical shape. |
| `$axis` | `int` | `0` | Position of the new axis in the result. |

**Returns:** `NDArray` of shape `(N, ...input_shape...)` if `$axis == 0`.

**Throws:** as `concatenate()`.

**Example:**

```php
$a = NDArray::fromArray([1, 2, 3]);
$b = NDArray::fromArray([4, 5, 6]);
print_r(NDArray::stack([$a, $b])->shape());
```

```
Array ( [0] => 2 [1] => 3 )
```

---

## Element-wise arithmetic (static)

These methods are also wired to the `+`, `-`, `*`, `/` operators. Each accepts two operands which may each be `NDArray` or scalar (`int`/`float`). Operands are broadcast to a common shape (see [broadcasting](../concepts/broadcasting.md)) and dtype is promoted (see [dtype promotion](../concepts/dtypes.md)).

### NDArray::add(): NDArray

Elementwise sum.

**Signature:** `public static function add(NDArray|int|float $a, NDArray|int|float $b): NDArray`

**Returns:** `NDArray` with the broadcast shape and promoted dtype.

**Throws:** `\ShapeException` for incompatible shapes; `\DTypeException` for unsupported operands.

**Example:**

```php
$a = NDArray::fromArray([1, 2, 3]);
$b = NDArray::fromArray([10, 20, 30]);
print_r(NDArray::add($a, $b)->toArray());
print_r(($a + 100)->toArray());        // operator form, scalar
```

```
Array ( [0] => 11 [1] => 22 [2] => 33 )
Array ( [0] => 101 [1] => 102 [2] => 103 )
```

---

### NDArray::subtract(): NDArray

Elementwise difference.

**Signature:** `public static function subtract(NDArray|int|float $a, NDArray|int|float $b): NDArray`

Same semantics and exceptions as `add()`.

---

### NDArray::multiply(): NDArray

Elementwise product.

**Signature:** `public static function multiply(NDArray|int|float $a, NDArray|int|float $b): NDArray`

Same semantics and exceptions as `add()`.

---

### NDArray::divide(): NDArray

Elementwise quotient.

**Signature:** `public static function divide(NDArray|int|float $a, NDArray|int|float $b): NDArray`

**Behaviour:**

- Float division by zero produces IEEE 754 results (`inf`, `nan`); no exception is thrown.
- Integer division by zero throws `\DivisionByZeroError`.

See [decision 7](../system.md).

---

## Element-wise math (instance)

The unary methods all return a new array of the same shape. Output dtype is the input dtype unless noted (`sqrt`, `exp`, `log*` always return float).

### NDArray::power(): NDArray

Raise each element to a scalar exponent.

**Signature:** `public function power(int|float $exponent): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$exponent` | `int\|float` | required | Scalar exponent. Array-valued exponents are deferred. |

**Returns:** `NDArray`.

**Example:**

```php
print_r(NDArray::fromArray([1, 2, 3, 4])->power(2)->toArray());
```

```
Array ( [0] => 1 [1] => 4 [2] => 9 [3] => 16 )
```

---

### NDArray::clip(): NDArray

Clamp each element to a range.

**Signature:** `public function clip(int|float|null $min = null, int|float|null $max = null): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$min` | `int\|float\|null` | `null` | Lower bound. `null` means no lower bound. |
| `$max` | `int\|float\|null` | `null` | Upper bound. `null` means no upper bound. |

**Returns:** `NDArray` of same shape and dtype.

**Throws:** `\NDArrayException` if both bounds are `null` (call would be a no-op — likely a bug).

---

### NDArray::sqrt(), exp(), log(), log2(), log10(), abs(), floor(), ceil()

Unary element-wise math. Each returns a new array of the same shape.

**Signature:** `public function NAME(): NDArray`

| Method | Output dtype |
|--------|--------------|
| `sqrt()` | always float |
| `exp()` | always float |
| `log()`, `log2()`, `log10()` | always float (natural / base-2 / base-10) |
| `abs()` | preserves input dtype |
| `floor()`, `ceil()` | preserves dtype (no-op for int dtypes) |

**Example:**

```php
print_r(NDArray::fromArray([1, 4, 9, 16])->sqrt()->toArray());
```

```
Array ( [0] => 1 [1] => 2 [2] => 3 [3] => 4 )
```

---

### NDArray::round(): NDArray

Round each element to a fixed number of decimal places, **half-away-from-zero**.

**Signature:** `public function round(int $decimals = 0): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$decimals` | `int` | `0` | Number of decimal places. May be negative (rounds to powers of ten). |

**Returns:** `NDArray` of same shape and dtype.

**Notes:**

- NumPHP rounds half-away-from-zero (PHP's `PHP_ROUND_HALF_UP`); NumPy rounds half-to-even. This is a deliberate divergence — see [round-half concept guide](../concepts/round-half.md) and [decision 8](../system.md).

**Example:**

```php
print_r(NDArray::fromArray([0.5, 1.5, 2.5, -0.5])->round()->toArray());
```

```
Array ( [0] => 1 [1] => 2 [2] => 3 [3] => -1 )
```

---

## BLAS (static)

These four methods route to OpenBLAS. Pure float32 inputs use the `s*` path; everything else promotes to float64 and uses the `d*` path. Integer inputs are promoted to float64 — there is no integer matmul in v1.

### NDArray::dot(): float|NDArray

1-D · 1-D = scalar (cblas_*dot). Other shapes are not supported by `dot` — use `matmul` for matrix product, `inner` / `outer` for vector products.

**Signature:** `public static function dot(NDArray $a, NDArray $b): float`

**Throws:** `\ShapeException` if either argument is not 1-D, or if lengths differ.

**Example:**

```php
$a = NDArray::fromArray([1.0, 2.0, 3.0]);
$b = NDArray::fromArray([4.0, 5.0, 6.0]);
echo NDArray::dot($a, $b), "\n";
```

```
32
```

---

### NDArray::matmul(): NDArray

Matrix product `(m,k) @ (k,n) -> (m,n)` (cblas_*gemm).

**Signature:** `public static function matmul(NDArray $a, NDArray $b): NDArray`

**Throws:** `\ShapeException` if either argument is not 2-D, or inner dimensions don't match.

**Notes:** 3-D+ batched matmul is deferred.

**Example:**

```php
$a = NDArray::fromArray([[1.0, 2.0], [3.0, 4.0]]);
$b = NDArray::fromArray([[5.0, 6.0], [7.0, 8.0]]);
print_r(NDArray::matmul($a, $b)->toArray());
```

```
Array ( [0] => Array ( [0] => 19 [1] => 22 )
        [1] => Array ( [0] => 43 [1] => 50 ) )
```

---

### NDArray::inner(): float

Same as `dot` for 1-D inputs.

**Signature:** `public static function inner(NDArray $a, NDArray $b): float`

---

### NDArray::outer(): NDArray

Outer product of two 1-D arrays — shape `(len(a), len(b))` (cblas_*ger).

**Signature:** `public static function outer(NDArray $a, NDArray $b): NDArray`

**Throws:** `\ShapeException` if either argument is not 1-D.

**Example:**

```php
$a = NDArray::fromArray([1.0, 2.0]);
$b = NDArray::fromArray([3.0, 4.0, 5.0]);
print_r(NDArray::outer($a, $b)->toArray());
```

```
Array ( [0] => Array ( [0] => 3 [1] => 4 [2] => 5 )
        [1] => Array ( [0] => 6 [1] => 8 [2] => 10 ) )
```

---

## Reductions

All reductions accept an optional `axis` and `keepdims`. Output dtype follows [decision 9](../system.md).

| Method | Default-axis (full reduction) returns | Per-axis returns | NaN behaviour |
|--------|---------------------------------------|------------------|---------------|
| `sum`, `mean`, `min`, `max` | scalar | `NDArray` | NaN propagates |
| `var`, `std` | scalar | `NDArray` | NaN propagates |
| `argmin`, `argmax` | int | `NDArray` of int64 | NaN-aware variants throw on all-NaN slice |

### NDArray::sum(): mixed

**Signature:** `public function sum(?int $axis = null, bool $keepdims = false): mixed`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$axis` | `?int` | `null` | If `null`, reduce over all axes. Otherwise reduce along that single axis. Negative indices count from the end. |
| `$keepdims` | `bool` | `false` | If `true`, the reduced axis is kept with size 1. |

**Returns:** `int|float` if reducing all axes, `NDArray` otherwise.

**Output dtype:** `int64` for int input, otherwise input's float dtype.

**Example:**

```php
$m = NDArray::fromArray([[1.0, 2.0], [3.0, 4.0]]);
echo $m->sum(), "\n";
print_r($m->sum(0)->toArray());     // axis 0
```

```
10
Array ( [0] => 4 [1] => 6 )
```

---

### NDArray::mean(): mixed
### NDArray::min(): mixed
### NDArray::max(): mixed
### NDArray::argmin(): mixed
### NDArray::argmax(): mixed

Identical signature to `sum()`. Behavioural notes:

- `mean` always returns float (`f64` for int input). Uses pairwise summation for numerical stability.
- `min` / `max` preserve the input dtype.
- `argmin` / `argmax` return `int64`.
- Any NaN in the input poisons the result for `mean`/`min`/`max`. Use the nan-variants below to skip NaNs.

---

### NDArray::var(): mixed
### NDArray::std(): mixed

Variance and standard deviation via Welford's online algorithm.

**Signature:** `public function var(?int $axis = null, bool $keepdims = false, int $ddof = 0): mixed`
**Signature:** `public function std(?int $axis = null, bool $keepdims = false, int $ddof = 0): mixed`

**Parameters:** as `sum()` plus `$ddof` (delta degrees of freedom — divisor is `N - ddof`).

**Output dtype:** float (`f64` for int input).

---

### NaN-aware variants

`nansum`, `nanmean`, `nanmin`, `nanmax`, `nanvar`, `nanstd`, `nanargmin`, `nanargmax` — same signatures as their plain counterparts, but NaN inputs are skipped instead of propagating. See [NaN policy](../concepts/nan-policy.md) and [decision 10](../system.md).

| Method | All-NaN slice |
|--------|---------------|
| `nansum` | `0` |
| `nanmean`, `nanmin`, `nanmax` | `NaN` |
| `nanvar`, `nanstd` | `NaN` |
| `nanargmin`, `nanargmax` | throws `\NDArrayException` |

For integer dtypes (which cannot hold NaN), the nan-variants are aliases of the plain forms.

**Example:**

```php
$x = NDArray::fromArray([1.0, NAN, 3.0]);
echo $x->mean(), "\n";          // NAN
echo $x->nanmean(), "\n";       // 2.0
```

```
NAN
2
```

---

## Cumulative reductions

Running totals along an axis. Output shape matches input when `$axis` is given; flattened to 1-D of size `size()` when `$axis === null`.

Output dtype follows [decision 31](../system.md): integer input promotes to `int64` (avoids silent overflow on `int32` accumulation, consistent with `sum`); `float32` and `float64` are preserved. **`cumprod` on integer input diverges from NumPy** — NumPy preserves the input dtype, NumPHP promotes to `int64`. Documented and locked.

### NDArray::cumsum(): NDArray

**Signature:** `public function cumsum(?int $axis = null): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$axis` | `?int` | `null` | If `null`, flatten then cumulate (output is 1-D). Otherwise cumulate along that axis. Negative indices count from the end. |

**Returns:** `NDArray` — same shape as input when `$axis` is an int, otherwise 1-D of length `size()`.

**Throws:** `\ShapeException` if `$axis` is out of range.

**Example:**

```php
$a = NDArray::fromArray([1.0, 2.0, 3.0, 4.0]);
print_r($a->cumsum()->toArray());           // [1, 3, 6, 10]

$m = NDArray::fromArray([[1, 2, 3], [4, 5, 6]]);
print_r($m->cumsum(1)->toArray());          // [[1, 3, 6], [4, 9, 15]]
```

```
Array ( [0] => 1 [1] => 3 [2] => 6 [3] => 10 )
Array ( [0] => Array ( [0] => 1 [1] => 3 [2] => 6 ) [1] => Array ( [0] => 4 [1] => 9 [2] => 15 ) )
```

---

### NDArray::cumprod(): NDArray

**Signature:** `public function cumprod(?int $axis = null): NDArray`

Same parameters and shape rules as `cumsum`, with multiplication instead of addition.

**Note:** integer input promotes to `int64` (NumPy preserves input dtype here — see [decision 31](../system.md)).

**Example:**

```php
$a = NDArray::fromArray([1, 2, 3, 4]);
print_r($a->cumprod()->toArray());          // [1, 2, 6, 24]
```

```
Array ( [0] => 1 [1] => 2 [2] => 6 [3] => 24 )
```

---

### NaN-aware variants — nancumsum, nancumprod

`nancumsum` / `nancumprod` skip NaN inputs by treating them as the additive (0) / multiplicative (1) identity. Default `cumsum`/`cumprod` propagate NaN — once a NaN enters the running accumulator, every subsequent output element along the axis is NaN.

| All-NaN slice | Result |
|---|---|
| `nancumsum` | all `0` |
| `nancumprod` | all `1` |

For integer dtypes, the nan-variants are aliases of the plain forms.

**Example:**

```php
$x = NDArray::fromArray([1.0, NAN, 3.0, 4.0]);
print_r($x->cumsum()->toArray());           // [1, NAN, NAN, NAN]
print_r($x->nancumsum()->toArray());        // [1, 1, 4, 8]
```

---

## Sort

### NDArray::sort(): NDArray

Return a sorted copy along the given axis. Uses `qsort` — **not stable** for equal keys (see [decision 11](../system.md)).

**Signature:** `public function sort(?int $axis = -1): NDArray`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$axis` | `?int` | `-1` | Axis to sort along. `-1` means the last axis. Explicit `null` means flatten then sort. |

**Returns:** `NDArray` of same shape and dtype.

**Example:**

```php
$x = NDArray::fromArray([3, 1, 4, 1, 5, 9, 2, 6]);
print_r($x->sort()->toArray());
```

```
Array ( [0] => 1 [1] => 1 [2] => 2 [3] => 3 [4] => 4 [5] => 5 [6] => 6 [7] => 9 )
```

---

### NDArray::argsort(): NDArray

Indices that would sort the array. Same parameters as `sort()`. Returns an `int64` array.

**Signature:** `public function argsort(?int $axis = -1): NDArray`

---

## I/O

### NDArray::save(): void

Write the array to a file in NumPHP's binary format.

**Signature:** `public function save(string $path): void`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$path` | `string` | required | File path. PHP stream wrappers are honoured. |

**Returns:** `void`.

**Throws:** `\NDArrayException` if the file cannot be opened or write fails.

**Notes:** The file format is little-endian only. Big-endian platforms refuse to compile (`#error`). See [decision 20](../system.md).

---

### NDArray::toCsv(): void

Write a 1-D or 2-D array to a CSV file. Locale-safe (decimal separator is always `.`).

**Signature:** `public function toCsv(string $path): void`

**Throws:** `\NDArrayException` if the array is more than 2-D, or if the write fails.

---

### NDArray::toArray(): array|int|float

Materialise as a (possibly nested) PHP array — a complete copy.

**Signature:** `public function toArray(): array|int|float`

**Returns:** PHP array nested to depth `ndim()`. For a 0-D array, returns the scalar.

---

## Interop

### NDArray::bufferView(): BufferView

Construct a [`BufferView`](bufferview.md) over this array's underlying buffer for FFI consumers.

**Signature:** `public function bufferView(bool $writeable = false): BufferView`

**Parameters:**

| Name | Type | Default | Description |
|------|------|---------|-------------|
| `$writeable` | `bool` | `false` | Advisory contract bit recorded on the view. Not enforced on the source in v1 — see [decision 22](../system.md). |

**Returns:** `BufferView` snapshotting `ptr`, `dtype`, `shape`, `strides`, and `writeable`. The view holds a refcount on the underlying buffer, so the buffer outlives the source array if the user drops the array first.

**Throws:** `\NDArrayException` if the array is not C-contiguous (transpose / non-contiguous views must be cloned first).

**Example:**

```php
$a = NDArray::arange(0, 10, 1, "float64");
$v = $a->bufferView();
echo $v->dtype, " ", $v->shape[0], " ", $v->writeable ? "rw" : "ro", "\n";
```

```
float64 10 ro
```

---

## See also

- [`Linalg`](linalg.md) — linear algebra static methods (inv/det/solve/svd/eig/norm).
- [`BufferView`](bufferview.md) — FFI bridge.
- [Exceptions](exceptions.md) — `\NDArrayException` and the three subclasses thrown above.
- [Concepts: dtype promotion](../concepts/dtypes.md), [broadcasting](../concepts/broadcasting.md), [views vs copies](../concepts/views-and-copies.md), [NaN policy](../concepts/nan-policy.md), [round-half](../concepts/round-half.md).
- [NumPy ↔ NumPHP cheatsheet](../cheatsheet-numpy.md).
