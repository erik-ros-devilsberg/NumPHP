# Story 3: Array Creation API

> Part of [Epic: NumPHP](epic-numphp.md)

**Outcome:** PHP userland can construct ndarrays.

## Class registration

```c
zend_class_entry *numphp_ndarray_ce;

PHP_MINIT_FUNCTION(numphp) {
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "NDArray", numphp_ndarray_methods);
    numphp_ndarray_ce = zend_register_internal_class(&ce);

    // Implements ArrayAccess, Countable (used by Story 4)
    zend_class_implements(numphp_ndarray_ce, 2,
        zend_ce_arrayaccess, zend_ce_countable);

    return SUCCESS;
}
```

## Method registration

```c
static const zend_function_entry numphp_ndarray_methods[] = {
    PHP_ME(NDArray, zeros,     arginfo_zeros,     ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
    PHP_ME(NDArray, ones,      arginfo_ones,      ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
    PHP_ME(NDArray, full,      arginfo_full,      ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
    PHP_ME(NDArray, eye,       arginfo_eye,       ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
    PHP_ME(NDArray, arange,    arginfo_arange,    ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
    PHP_ME(NDArray, fromArray, arginfo_fromArray, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
    PHP_FE_END
};
```

## API

- `NDArray::zeros(array $shape, string $dtype = 'float64')`
- `NDArray::ones(array $shape, string $dtype = 'float64')`
- `NDArray::full(array $shape, float|int $value, string $dtype = 'float64')`
- `NDArray::eye(int $n, ?int $m = null, int $k = 0, string $dtype = 'float64')` — identity / off-diagonal; `m` defaults to `n`, `k` shifts the diagonal
- `NDArray::arange(int|float $start, int|float $stop, int|float $step = 1, ?string $dtype = null)` — dtype defaults to `int64` if all args are int, else `float64`
- `NDArray::fromArray(array $data, ?string $dtype = null)` — recursive, infers shape, validates uniformity

## fromArray dtype inference
- All-int input → `int64`
- Any float in input → `float64`
- Explicit `$dtype` overrides inference and coerces values
- Mixed types follow the Story 1 promotion rules

## fromArray validation
- Walks nested PHP array recursively
- Infers shape from nesting depth and lengths
- All branches at a given depth must be the same length — ragged → `\ShapeException`
- Coerces PHP scalars to target dtype

## Memory note
`calloc` for zeros (also IEEE-754-zero for floats); `malloc` + typed fill loop for ones / full.
