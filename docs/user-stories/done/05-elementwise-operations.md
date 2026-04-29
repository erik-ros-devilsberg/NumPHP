# Story 5: Element-wise Operations

> Part of [Epic: NumPHP](epic-numphp.md)

**Outcome:** Basic arithmetic works across arrays, including broadcast cases.

> **Sequencing note:** This story consumes the nd-iterator built in Story 7. Implement Story 7 first, then build element-wise loops on top of it. Doing element-wise first means rewriting every typed loop once broadcasting lands.

## Core loop pattern

```c
PHP_METHOD(NDArray, add) {
    zval *a_zval, *b_zval;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJECT_OF_CLASS(a_zval, numphp_ndarray_ce)
        Z_PARAM_OBJECT_OF_CLASS(b_zval, numphp_ndarray_ce)
    ZEND_PARSE_PARAMETERS_END();

    numphp_ndarray *a = get_ndarray_from_zval(a_zval);
    numphp_ndarray *b = get_ndarray_from_zval(b_zval);

    numphp_dtype out_dt = promote_dtype(a->dtype, b->dtype);   // Story 1 table
    numphp_ndarray *out = alloc_broadcast_result(a, b, out_dt); // Story 7 rules

    numphp_nditer it;                                           // Story 7
    nditer_init_3(&it, a, b, out);
    dispatch_add(&it, out_dt);
}
```

## Typed dispatch

Per (output) dtype, separate loop body so the compiler can auto-vectorise with SIMD:

```c
switch (out_dt) {
    case NUMPHP_FLOAT64: {
        while (!it.done) {
            *(double *)it.ptr_out =
                read_as_f64(it.ptr_a, a->dtype) + read_as_f64(it.ptr_b, b->dtype);
            nditer_next(&it);
        }
        break;
    }
    case NUMPHP_FLOAT32: /* ... */ break;
    case NUMPHP_INT64:   /* ... */ break;
    case NUMPHP_INT32:   /* ... */ break;
}
```

When both inputs match `out_dt` exactly, skip `read_as_*` and read directly — the hot path.

## Operator overloading

```c
numphp_ndarray_object_handlers.do_operation = numphp_do_operation;

static int numphp_do_operation(zend_uchar opcode, zval *result, zval *op1, zval *op2) {
    switch (opcode) {
        case ZEND_ADD:        return op_add(result, op1, op2);
        case ZEND_SUB:        return op_sub(result, op1, op2);
        case ZEND_MUL:        return op_mul(result, op1, op2);
        case ZEND_DIV:        return op_div(result, op1, op2);
        case ZEND_ASSIGN_ADD: return op_add_inplace(op1, op2);
        case ZEND_ASSIGN_SUB: return op_sub_inplace(op1, op2);
        case ZEND_ASSIGN_MUL: return op_mul_inplace(op1, op2);
        case ZEND_ASSIGN_DIV: return op_div_inplace(op1, op2);
        default:              return FAILURE;
    }
}
```

`+=` and friends route to in-place variants, which write into `op1->buffer` directly.

## Scalar-array variants

When one operand is a PHP scalar, the loop reads it once and applies it across the iterator. No allocation for the scalar side; output dtype is the promotion of the array's dtype with the scalar's PHP type (int → `i64`, float → `f64`).

API surface:
- `NDArray::add($a, $b)` — `$b` may be `NDArray` or scalar
- `NDArray::subtract($a, $b)`
- `NDArray::multiply($a, $b)`
- `NDArray::divide($a, $b)`
- In-place: `addInplace`, `subtractInplace`, `multiplyInplace`, `divideInplace`
- Operator overloading via `do_operation`

## Division by zero
- Float dtypes: IEEE 754 (`inf`, `-inf`, `nan`); no exception.
- Int dtypes: `\DivisionByZeroError` thrown, no result allocated.

## Shape & broadcast
Shape compatibility is delegated to Story 7's broadcast rules + iterator. This story does not re-implement matched-shape-only loops.
