# Story 6: Shape Manipulation

> Part of [Epic: NumPHP](epic-numphp.md)

**Outcome:** Arrays can be reshaped without copying data where possible.

## reshape

Change shape without changing data. Total element count must stay the same.

```
[[1, 2, 3],      →    [1, 2, 3, 4, 5, 6]
 [4, 5, 6]]
shape: [2, 3]    →    shape: [6]
```

- If array is C-contiguous → return view, recalculate strides, no copy.
- If array is non-contiguous (e.g. after transpose) → copy first (C-contiguous), then reshape.
- `-1` placeholder in shape is inferred from the remaining product: `reshape([2, -1])` on a 12-element array → `[2, 6]`. Multiple `-1` entries → `\ShapeException`.

## transpose

Flip axes. Free — no data moves.

```
[[1, 2, 3],      →    [[1, 4],
 [4, 5, 6]]            [2, 5],
                        [3, 6]]
shape: [2, 3]    →    shape: [3, 2]
```

Default — reverse all axes:
```c
for (int i = 0; i < ndim; i++) {
    new_shape[i]   = old_shape[ndim - 1 - i];
    new_strides[i] = old_strides[ndim - 1 - i];
}
```

Explicit axis permutation: `$a->transpose([0, 2, 1])` swaps axes 1 and 2 of a 3D array. Permutation must be a complete permutation of `[0, ndim)` — missing or duplicate axis → `\ShapeException`.

After transpose: `flags &= ~C_CONTIGUOUS`; `F_CONTIGUOUS` set when the input was C-contiguous.

Real use case: ML frameworks expect images as `[channels, height, width]`, OpenCV delivers `[height, width, channels]`. Transpose converts between them at zero cost.

## flatten

Collapse any shape to 1D. Always returns a C-contiguous copy.

```
[[[1, 2], [3, 4]],
 [[5, 6], [7, 8]]]

shape: [2, 2, 2]  →  [1, 2, 3, 4, 5, 6, 7, 8]  shape: [8]
```

Copy is mandatory — after operations like transpose, data in memory may not be in expected order. Flatten guarantees a clean contiguous 1D buffer.

In practice, reshape to a 2D array is often preferable when structure matters — e.g. `[6, 2]` keeps pairs intact.

## squeeze / expand_dims
- `squeeze(?int $axis = null)` — remove size-1 dimensions; named axis must already be size 1 or `\ShapeException`.
- `expand_dims(int $axis)` — insert size-1 dimension at given axis.

## concatenate / stack

- `NDArray::concatenate(array $arrays, int $axis = 0)` — joins along an existing axis. All other axes' sizes must match exactly. dtype is the promotion across all inputs (Story 1 table).
- `NDArray::stack(array $arrays, int $axis = 0)` — adds a new axis. All input shapes must be identical. Result has `ndim + 1` dimensions.

Both copy into a new C-contiguous buffer.
