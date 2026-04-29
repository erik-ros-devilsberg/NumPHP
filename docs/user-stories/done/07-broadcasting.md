# Story 7: Broadcasting & nd-iterator

> Part of [Epic: NumPHP](epic-numphp.md)

**Outcome:** A reusable nd-iterator and broadcasting rules. Stories 5, 6, 8, 9, 10 all build on it.

> This is the hardest story. Do not underestimate it.

## The concept

Broadcasting lets a smaller array be "stretched" to match a larger one. No data is copied.

```
A: [[1, 2, 3],
    [4, 5, 6],
    [7, 8, 9]]   shape: [3, 3]

B: [10, 20, 30]  shape: [3]

B broadcasts to every row of A:
→ [[11, 22, 33],
   [14, 25, 36],
   [17, 28, 39]]
```

## The rules

1. Pad shapes on the left with 1s until ndim matches.
2. Each dimension must be equal, or one of them must be 1.
3. Output shape = max of each dimension pair.

```
A shape: [3, 1, 4]
B shape:    [5, 4]  →  [1, 5, 4]
Result:  [3, 5, 4]
```

Incompatible — `[3, 4]` and `[4, 3]` — neither dimension is 1, sizes differ → `\ShapeException`.

## Implementation: virtual strides

Set stride to `0` for broadcast dimensions. The loop index advances but the pointer doesn't:

```c
if (in->shape[dim] == 1 && out->shape[dim] > 1) {
    broadcast_strides[dim] = 0;
} else {
    broadcast_strides[dim] = in->strides[dim];
}
```

## The iterator

```c
#define NUMPHP_MAX_NDIM 16
#define NUMPHP_ITER_MAX_OPERANDS 4

typedef struct {
    int        ndim;
    int        nop;                                  // number of operands
    zend_long  shape[NUMPHP_MAX_NDIM];               // broadcast result shape
    zend_long  index[NUMPHP_MAX_NDIM];               // current position per dim
    zend_long  strides[NUMPHP_ITER_MAX_OPERANDS][NUMPHP_MAX_NDIM]; // 0 for broadcast dims
    char      *ptr[NUMPHP_ITER_MAX_OPERANDS];        // current byte pointer per operand
    char      *base[NUMPHP_ITER_MAX_OPERANDS];       // start of each operand
    bool       done;
} numphp_nditer;

void  nditer_init(numphp_nditer *it, int nop, numphp_ndarray **ops, numphp_ndarray *out);
void  nditer_next(numphp_nditer *it);   // advances index + ptr[] across all operands
```

`nditer_next` increments the inner-most axis first, carries when it overflows, and adjusts each operand's pointer using its own (possibly zero) stride for that dimension.

## In-place output guard

If the output is one of the inputs (in-place op) and its broadcast strides include a `0`, multiple iterations write to the same byte — undefined behavior. Reject before iterating:

```c
if (output_aliases_input(out, in) && has_zero_stride(strides[out_idx])) {
    zend_throw_exception(numphp_shape_exception_ce,
        "In-place operation requires non-broadcasting output", 0);
    return FAILURE;
}
```

## Build order

The iterator is built **first** in this story. Element-wise (Story 5), reductions (Story 9), and any future axis-aware op consume it. Anything written before the iterator gets rewritten — don't.
