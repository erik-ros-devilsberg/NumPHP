# Story 11: Interoperability

> Part of [Epic: NumPHP](epic-numphp.md)

**Outcome:** NumPHP can exchange data with PHP, with FFI consumers, and (optionally) the Arrow ecosystem.

This story is large enough to ship in three phases. Each phase is independently shippable and the later phases can move to their own sprint.

**Status:** Phases A and B shipped 2026-04-29. Phase C (Arrow IPC) remains.

## Phase A — PHP arrays & file I/O (essential) ✅ shipped 2026-04-29

Depends on Story 3 (creation API).

- `$a->toArray(): array` — recursive nested array, dtype-coerced to PHP scalars.
- `NDArray::fromArray(array $data, ?string $dtype = null)` — already in Story 3; this phase just verifies round-trip with `toArray`.
- `NDArray::fromCsv(string $path, string $dtype = 'float64', bool $header = false)` — minimal, single-dtype CSV reader.
- `$a->toCsv(string $path)` — writes 1D or 2D arrays; higher dims rejected.
- `NDArray::load(string $path)` / `$a->save(string $path)` — simple binary format: magic header (`"NUMPHP\0\1"`), dtype byte, ndim byte, shape array, raw little-endian buffer.

## Phase B — Raw binary buffer (FFI surface) ✅ shipped 2026-04-29

For trusted internal callers using PHP's `FFI` extension or C-side embedding.

```php
final class BufferView {
    public int    $ptr;        // void* as int (use with FFI::cast)
    public string $dtype;
    public array  $shape;
    public array  $strides;
    public bool   $writeable;
}

$bv = $a->bufferView(bool $writeable = false);
```

**Risks**
- Buffer overflows if consumer reads beyond bounds.
- Use-after-free if the NDArray is GC'd while the consumer holds the pointer.
- No dtype/shape travels with the pointer at the C boundary.

**Mitigations**
- The `BufferView` zval holds a refcount on the underlying `numphp_buffer` (Story 2). The buffer outlives the view.
- `writeable = false` clears the array's `WRITEABLE` flag while the view exists; set on view destruction.
- Document that callers crossing FFI assume responsibility for bounds.
- `bufferView()` only available on C-contiguous arrays; throws `\NDArrayException` otherwise (caller must `copy()` first).

## Phase C — Arrow IPC (optional, soft dependency)

Self-describing format: shape, dtype, metadata travel with the data. Safe for cross-process and cross-language exchange. Enables interop with Python, R, DuckDB, Pandas.

- Add `PHP_CHECK_LIBRARY(arrow, ...)` to `config.m4` as **optional** — if absent, the Arrow methods are not registered, the rest of the extension still builds.
- API: `NDArray::fromArrow(string $ipc): NDArray`, `$a->toArrow(): string`.
- Initial scope: 1D and 2D arrays of primitive dtypes only. Nested types deferred.

> **Open question (carries over from epic):** PHP has no equivalent to Python's buffer protocol. NumPHP designs this from scratch — opportunity to do it more safely and idiomatically. The `BufferView` shape above is the v1 attempt.
