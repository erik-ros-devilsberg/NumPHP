# BufferView

`BufferView` is the FFI bridge: a metadata-and-pointer snapshot over an `NDArray`'s underlying buffer. It is the canonical way for PHP code to hand a NumPHP array's memory to the [PHP FFI extension](https://www.php.net/manual/en/book.ffi.php) without copying.

The class is `final`. Its constructor is `private` — instances must be obtained via [`NDArray::bufferView()`](ndarray.md#ndarraybufferview-bufferview).

```php
$arr = NDArray::arange(0, 100, 1, "float64");
$view = $arr->bufferView();   // not new BufferView(...)
```

---

## Public properties

All five properties are read-only by convention. They are populated once at construction and never refreshed (decision 23).

| Property | Type | Description |
|----------|------|-------------|
| `$ptr` | `int` | Address of the first byte of the buffer, as a PHP integer. Pass to `FFI::cast` or similar to interpret as a typed pointer. |
| `$dtype` | `string` | One of `"float32"`, `"float64"`, `"int32"`, `"int64"`. |
| `$shape` | `array` | Dimensions, captured from the source array at construction time. |
| `$strides` | `array` | Stride along each axis, in bytes. |
| `$writeable` | `bool` | Advisory contract bit. See decision 22 — does not enforce read-only on the source in v1. |

---

## Lifetime

`BufferView` holds a refcount on the source array's `numphp_buffer`. The buffer remains alive as long as either the source `NDArray` *or* any `BufferView` over it is reachable. This means it is safe to drop the source array first:

```php
$arr  = NDArray::arange(0, 8, 1, "float64");
$view = $arr->bufferView();
unset($arr);                  // buffer still alive — view holds it
// $view->ptr is still valid here
```

---

## Construction constraints

`bufferView()` requires the source to be **C-contiguous**. Transposed and other non-contiguous views must be cloned first:

```php
$m = NDArray::arange(0, 6)->reshape([2, 3]);
$t = $m->transpose();             // non-contig view
// $t->bufferView();              // throws \NDArrayException
$t->reshape($t->shape())->bufferView();   // OK after a contiguous copy
```

The `clone $arr` syntax also produces a contiguous copy if the source is non-contiguous.

---

## Mutability semantics

`$writeable` is **advisory**. It is recorded on the view but does not flip a flag on the source array. This is documented as a v1 limitation in [decision 22](../system.md). Treat the value as a contract you make with the FFI consumer:

- If you pass `bufferView(true)` to FFI code, you assert the consumer is allowed to write.
- If you pass `bufferView(false)`, you assert the consumer should treat the memory as read-only.

Hard enforcement (clearing `WRITEABLE` on the source while a `writeable=false` view is outstanding) would require a per-array view counter; deferred until a user needs it.

---

## Metadata is a snapshot

Once a view is constructed, its `$shape`, `$strides`, `$dtype`, `$ptr`, `$writeable` properties are frozen. If the source is reshaped / transposed afterwards, the view's properties become stale relative to the source — but the underlying buffer pointer remains valid (refcount keeps it alive). Re-create the view if the source's logical structure changes.

---

## Example: zero-copy handoff to FFI

```php
// snippet-test: skip — requires the FFI extension, which is not loaded in the test SAPI
$arr  = NDArray::arange(0, 1000, 1, "float64");
$view = $arr->bufferView(writeable: true);

$ffi = FFI::cdef("typedef struct { double* data; size_t n; } vec_t;");
$vec = $ffi->new("vec_t");
$vec->data = $ffi->cast("double*", $view->ptr);
$vec->n    = $view->shape[0];

// Pass $vec to a C function loaded via FFI…
```

When the FFI extension is not loaded, `BufferView` itself still works — it is the consumer-side library that needs FFI, not NumPHP.

---

## See also

- [`NDArray::bufferView()`](ndarray.md#ndarraybufferview-bufferview) — factory.
- [`docs/system.md`](../system.md) decisions 22-24 — covers the lifetime / advisory-writeable / property-default constraints behind this class.
