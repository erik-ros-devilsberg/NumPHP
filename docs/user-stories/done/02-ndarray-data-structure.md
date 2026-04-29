# Story 2: ndarray Data Structure

> Part of [Epic: NumPHP](epic-numphp.md)

**Outcome:** Core C struct that all operations build on. Decisions from Story 1 (dtypes, promotion table, exception hierarchy, memory model) are realised here.

## The data buffer (refcounted)

The buffer is decoupled from the array shell so that views can outlive their owner safely.

```c
typedef struct {
    void      *data;       // raw contiguous memory block
    zend_long  nbytes;     // total allocation in bytes
    zend_long  refcount;   // owners + views holding this buffer
} numphp_buffer;
```

Owners and views both bump/drop `refcount`; the buffer is freed when it hits zero.

## The array struct

```c
typedef enum {
    NUMPHP_FLOAT32,
    NUMPHP_FLOAT64,
    NUMPHP_INT32,
    NUMPHP_INT64
} numphp_dtype;

#define NUMPHP_C_CONTIGUOUS  (1u << 0)
#define NUMPHP_F_CONTIGUOUS  (1u << 1)
#define NUMPHP_WRITEABLE     (1u << 2)

typedef struct {
    numphp_buffer *buffer;     // refcounted; never NULL
    zend_long     *shape;
    zend_long     *strides;    // byte offsets per dimension
    zend_long      offset;     // bytes from buffer->data to element [0]
    int            ndim;
    numphp_dtype   dtype;
    zend_long      itemsize;
    zend_long      size;       // total element count
    unsigned       flags;      // C/F contiguous, writeable
} numphp_ndarray;
```

`offset` lets a view land anywhere inside the buffer (necessary for `slice`, transposed views).

## Zend object wrapper

```c
typedef struct {
    numphp_ndarray *array;
    zend_object     std;       // must be last
} numphp_ndarray_object;

static inline numphp_ndarray_object *
numphp_obj_from_zo(zend_object *obj) {
    return (numphp_ndarray_object *)
        ((char *)obj - XtOffsetOf(numphp_ndarray_object, std));
}
```

Register handlers for create, free, clone.

## Clone semantics
- **Owner clone** → deep copy of buffer, new contiguous layout, `WRITEABLE` set.
- **View clone** → still produces a deep copy (PHP's `clone` is value semantics; users opt into views via slicing/indexing, not `clone`).
- `copy()` method is the explicit deep-copy escape hatch and always returns C-contiguous.

## Strides explained

Strides tell you how many bytes to jump to reach the next element in each dimension.

For a `[3, 4]` float64 array (8 bytes per element):
- Next column: 8 bytes → stride = `8`
- Next row: 4 elements × 8 bytes → stride = `32`
- Strides = `[32, 8]`

Element access: `buffer->data + offset + (row * 32) + (col * 8)`

**Key insight:** strides + offset are what make reshape and transpose free operations — you change the metadata, not the data.

## Constraints
- All dimensions must be uniform — ragged arrays are invalid.
- A `[2, 3]` array must have exactly 3 elements in every row.
- `fromArray` rejects ragged input with `\ShapeException` (Story 1 hierarchy).

## Cross-cutting
- dtype promotion follows the table in Story 1.
- All allocation paths set `flags` correctly; helper `numphp_recompute_contiguity()` derives the bits from `shape`/`strides`/`itemsize`.
