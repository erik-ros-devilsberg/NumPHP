# Broadcasting

Broadcasting is the rule NumPHP applies when an operation receives two arrays of different shapes. Rather than requiring every operand to have the exact same shape, broadcasting describes the conditions under which a smaller array is "stretched" across a larger one — without copying — so that elementwise operations can proceed.

The rules are identical to NumPy's. If you know NumPy broadcasting, you know NumPHP broadcasting.

---

## The rule (in three lines)

Two shapes are broadcast-compatible if, when right-aligned, every pair of dimensions either:

1. is equal,
2. has one of them equal to 1, or
3. has one of them missing (treat missing as 1).

If broadcastable, the result shape is the elementwise maximum of the right-aligned dimensions.

---

## Worked example

```
$a shape:    (    3, 4)
$b shape:    (5, 1, 4)
right-aligned and compared:
                3 vs 1   → 3   (b stretches)
                4 vs 4   → 4
                _ vs 5   → 5   (a missing → treat as 1, then stretch)
result shape:        (5, 3, 4)
```

The broadcast is virtual — `$b` is not copied 5×3 = 15 times. The elementwise loop reads each value from `$b` repeatedly via virtual zero strides for the broadcast dimensions. See [decision 14 in system.md](../system.md) and the `numphp_nditer` implementation for the C-side details.

---

## Compatible vs incompatible shapes

| Shape A | Shape B | Result |
|---------|---------|--------|
| `(3,)` | `(3,)` | `(3,)` ✓ — identical |
| `(3,)` | `()` | `(3,)` ✓ — scalar against vector |
| `(3, 4)` | `(4,)` | `(3, 4)` ✓ — vector across each row |
| `(3, 4)` | `(3, 1)` | `(3, 4)` ✓ — column vector across each column |
| `(3, 1)` | `(1, 4)` | `(3, 4)` ✓ — outer-product-like |
| `(3, 4)` | `(2, 4)` | **error** — `3 vs 2`, neither is 1 |
| `(3, 4)` | `(3, 5)` | **error** — `4 vs 5`, neither is 1 |

A shape mismatch raises `\ShapeException` with both shapes named in the message.

---

## Examples

### Scalar against an array

```php
$a = NDArray::fromArray([1, 2, 3]);
print_r(($a + 10)->toArray());
```

```
Array ( [0] => 11 [1] => 12 [2] => 13 )
```

### Row vector across a matrix

```php
$m = NDArray::fromArray([[1, 2, 3], [4, 5, 6]]);   // (2, 3)
$v = NDArray::fromArray([10, 20, 30]);             // (3,)
print_r(($m + $v)->toArray());
```

```
Array ( [0] => Array ( [0] => 11 [1] => 22 [2] => 33 )
        [1] => Array ( [0] => 14 [1] => 25 [2] => 36 ) )
```

### Column vector across a matrix

```php
$m = NDArray::fromArray([[1, 2, 3], [4, 5, 6]]);   // (2, 3)
$c = NDArray::fromArray([[100], [200]]);            // (2, 1)
print_r(($m + $c)->toArray());
```

```
Array ( [0] => Array ( [0] => 101 [1] => 102 [2] => 103 )
        [1] => Array ( [0] => 204 [1] => 205 [2] => 206 ) )
```

### Outer product via broadcast

`(3, 1) + (1, 4) → (3, 4)` is the broadcast pattern that produces an outer-style addition table:

```php
$row = NDArray::fromArray([1, 2, 3])->expandDims(1);    // (3, 1)
$col = NDArray::fromArray([10, 20, 30, 40])->expandDims(0);  // (1, 4)
print_r(($row + $col)->toArray());
```

```
Array ( [0] => Array ( [0] => 11 [1] => 21 [2] => 31 [3] => 41 )
        [1] => Array ( [0] => 12 [1] => 22 [2] => 32 [3] => 42 )
        [2] => Array ( [0] => 13 [1] => 23 [2] => 33 [3] => 43 ) )
```

---

## Common pitfalls

### Forgetting to add a dimension

A 1-D vector and a 2-D matrix will broadcast across rows by default (the 1-D vector is right-aligned and matches the trailing axis). To broadcast across **columns** instead, the vector must be reshaped to a column:

```php
$m = NDArray::fromArray([[1, 2, 3], [4, 5, 6]]);   // (2, 3)
$v = NDArray::fromArray([10, 20]);                  // (2,)
// $m + $v throws — (2,) right-aligned vs (2, 3) gives 2 vs 3.
$v_col = $v->expandDims(1);                         // (2, 1)
print_r(($m + $v_col)->toArray());                  // OK
```

### Confusing transpose with reshape

`transpose` rearranges axes (logical layout); `reshape` flattens and re-groups elements. For broadcasting, what matters is the *shape* — and how it right-aligns against the other operand.

### Implicit broadcast cost

Although broadcasting is conceptually a stretch, the actual elementwise loop iterates over every element of the result shape. Adding a `(1,)` to a `(10000, 10000)` array costs the same as adding a fully-allocated `(10000, 10000)` array. Broadcast saves memory, not work.

---

## Operators that broadcast

The arithmetic operators and the corresponding static methods all broadcast:

- `+`, `-`, `*`, `/` and `NDArray::add`, `subtract`, `multiply`, `divide`
- `power` (when the exponent is a scalar — array-valued exponents are deferred)

`offsetSet` accepts either a scalar (which is broadcast across the slot) or an NDArray of matching shape; it does not perform the right-align broadcast described above.

---

## See also

- [Decision 14](../system.md) — the nd-iterator design.
- [Decision 3](../system.md) — dtype promotion (applied alongside broadcasting).
- [`NDArray` API reference](../api/ndarray.md) — every elementwise method's parameters page describes its broadcast behaviour.
