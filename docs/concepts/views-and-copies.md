# Views and Copies

A NumPHP operation produces one of two things: a **view** — a new `NDArray` object that shares the underlying buffer with its source — or a **copy** — a new `NDArray` with its own buffer.

Views are cheap: O(1), no data allocation, just a new metadata shell. Mutations through a view are visible to the source (and vice versa). Copies are independent: O(N), and changes to either side are isolated.

This page documents which operations produce which, and how to force the one you want.

---

## The contract

| Operation | Returns |
|-----------|---------|
| `slice($start, $stop, $step)` | view |
| `transpose($axes)` | view |
| `squeeze($axis)` | view |
| `expandDims($axis)` | view |
| `reshape($shape)` | view if source is C-contiguous, copy otherwise |
| `offsetGet($i)` for n-D source | view (sub-array) |
| `offsetGet($i)` for 1-D source | scalar (no view, no copy — extracted) |
| `flatten()` | always copy |
| `toArray()` | always copy (and converts to PHP nested array) |
| `clone $arr` | copy (always C-contiguous) |
| `concatenate`, `stack` | always copy |
| `add` / `subtract` / etc. | always new array (no view sharing) |
| Any `Linalg::*` | new array, fresh buffer |
| `$arr->fromArray($php_array)` | copy of the input PHP array |
| Any `NDArray::zeros`/`ones`/`full`/`eye`/`arange` | fresh allocation |

Anything not listed above is a copy.

---

## Memory model

The buffer (the contiguous block of `dtype`-sized elements) is refcounted. Each `NDArray` object holds a refcount on its buffer. Views increment the refcount on construction; both owners and views decrement on destruction. The buffer is freed when the last refcount drops.

There is no copy-on-write in v1. Views remain views even when mutated — mutations through a view are immediately visible to all other views and the original owner. Decision 5 in [`docs/system.md`](../system.md) records this choice.

---

## Verifying with an example

```php
$a = NDArray::arange(0, 6)->reshape([2, 3]);  // owner
$row0 = $a[0];                                 // view of row 0 (shape (3,))

$row0[1] = 999;   // mutate through the view

print_r($a->toArray());
```

```
Array ( [0] => Array ( [0] => 0 [1] => 999 [2] => 2 )
        [1] => Array ( [0] => 3 [1] => 4   [2] => 5 ) )
```

The mutation is visible in `$a` because `$row0` shares the buffer.

---

## Forcing a copy

If you need a copy of a view — to mutate it without affecting the source — use any of:

- `clone $view` — produces a fresh, C-contiguous copy.
- `$view->reshape($view->shape())` — reshape to the same shape forces materialisation when the source is non-contiguous.
- `$view->flatten()->reshape($view->shape())` — explicit "make me a buffer-owning array."

`clone` is the most idiomatic.

---

## Forcing a view

Most cheap-shape operations already return views. The one that may return a copy is `reshape`: if the source is not C-contiguous, `reshape` materialises a fresh contiguous copy because the strides cannot be expressed as a view of the original layout. To get a view, ensure the source is C-contiguous first (e.g. by avoiding intermediate transposes).

---

## Contiguity flags

Every `NDArray` carries a flags bitfield with three bits relevant here:

| Flag | Meaning |
|------|---------|
| `C_CONTIGUOUS` | row-major; trailing axis has unit stride |
| `F_CONTIGUOUS` | column-major; leading axis has unit stride |
| `WRITEABLE` | element writes through this object are allowed |

`C_CONTIGUOUS` is what most operations check before returning a view. A 1-D array is always both C and F contiguous. A 2-D array transposed via `transpose()` is F-contiguous but not C-contiguous — that's why the `BufferView` factory rejects transposes (it requires C-contiguity for the FFI consumer to walk the buffer linearly).

`WRITEABLE` is informational in v1. The `BufferView` `$writeable` parameter is advisory and does not flip this bit. See [decision 22](../system.md).

---

## Pitfalls

### Modifying a slice loop counter

```php
$row = $matrix[$i];          // view
$row[0] = $value;             // mutates $matrix at $matrix[$i][0]
```

This is correct and intended. Confusion arises when the writer expects PHP value semantics (where `$row` would be a copy). NDArray views break that expectation; the writer must opt out via `clone` if isolation is desired.

### Returning a view from a function

If a function returns `$arr->slice(...)` and the caller stores it past the lifetime of `$arr`, the view keeps the buffer alive (refcounting works through views). This is safe — but the apparent cost of the view is now linked to the buffer's full size, not the slice's logical size.

### `flatten()` is a copy

`flatten()` always copies — even when the source is C-contiguous. This is deliberate: callers expect `flatten()` to return an independent 1-D array. If you want a 1-D view for the C-contiguous case, use `reshape([size()])` instead.

---

## See also

- [Decision 5](../system.md) — memory ownership model.
- [Decision 6](../system.md) — contiguity flags.
- [Decision 22](../system.md) — `BufferView` `$writeable` is advisory in v1.
- [`BufferView` API reference](../api/bufferview.md).
