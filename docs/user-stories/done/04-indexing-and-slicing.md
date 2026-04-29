# Story 4: Indexing & Slicing

> Part of [Epic: NumPHP](epic-numphp.md)

**Outcome:** Elements and subarrays are accessible. Multi-dim writes work.

## The PHP problem
`ArrayAccess` only gives `offsetGet($offset)` / `offsetSet($offset, $value)` — no native range syntax. `$a[1:3]` is impossible.

## Solution
- Integer indexing via `ArrayAccess`: `$a[0]`, `$a[1]`
- Slice via explicit method: `$a->slice(int $start, int $stop, int $step = 1)`
- Negative indexing supported on both
- Multi-axis slicing via tuple form: `$a->slice([0, 3], [1, 4])` selects rows 0–2, cols 1–3

## offsetGet handler

```c
// Returns scalar zval for full 1D index, NDArray view for partial nD index
static zval *handler_offset_get(zend_object *obj, zval *offset, int type, zval *rv) {
    numphp_ndarray *a = numphp_obj_from_zo(obj)->array;
    zend_long idx = normalize_index(offset, a->shape[0]);  // negative → wrap

    if (a->ndim == 1) {
        // scalar: read using strides[0] from buffer->data + offset
        return scalar_zval_at(rv, a, idx);
    }
    // view:
    //   buffer = a->buffer (refcount++)
    //   offset = a->offset + idx * strides[0]
    //   shape  = shape[1..]
    //   strides = strides[1..]
    //   ndim   = ndim - 1
    //   flags  = recompute (WRITEABLE inherited)
    return new_view(rv, a, idx);
}
```

## offsetSet handler
Required for `$a[i] = ...` and (via chained views) `$a[i][j] = ...`. Without it, views are read-only from PHP.

```c
static void handler_offset_set(zend_object *obj, zval *offset, zval *value) {
    numphp_ndarray *a = numphp_obj_from_zo(obj)->array;
    if (!(a->flags & NUMPHP_WRITEABLE)) {
        zend_throw_exception(numphp_index_exception_ce, "Array is read-only", 0);
        return;
    }
    // Scalar destination → coerce value to dtype, write at computed byte offset.
    // View destination → broadcast-assign value (Story 7 broadcasting applies).
}
```

## Views and refcount
A view holds a refcount on the parent's `numphp_buffer` (Story 2). The view's `offset` is the parent's `offset + idx * strides[0]`. Owner GC does not invalidate live views.

## Negative indexing
Normalised at the boundary: `idx < 0 ? idx + shape[d] : idx`. Out of range after normalisation → `\IndexException`.

## Deferred (post-v1)
- Boolean mask indexing (`$a[$mask]`)
- Integer-array fancy indexing (`$a->take([0, 3, 5])` is the explicit-method workaround for v1)
