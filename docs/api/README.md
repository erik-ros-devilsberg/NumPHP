# NumPHP API Reference

Complete reference for every public class and method in the `numphp` extension. If a method is documented here, it is part of the v1 API contract.

## Classes

| Class | Description |
|-------|-------------|
| [`NDArray`](ndarray.md) | The n-dimensional array. Creation, indexing, shape, math, reductions, BLAS, I/O. |
| [`Linalg`](linalg.md) | Static linear algebra routines backed by LAPACK. |
| [`BufferView`](bufferview.md) | FFI bridge — exposes the underlying buffer pointer + metadata. |
| [Exceptions](exceptions.md) | `\NDArrayException` and the three subclasses thrown across the API. |

## Format conventions

Every method entry in this reference follows the same template:

```
### ClassName::methodName(): ReturnType

One-line description.

**Signature:**
public [static] function methodName(<params>): <return-type>

**Parameters:**
| Name | Type | Default | Description |
|------|------|---------|-------------|

**Returns:** <type> — <description>

**Throws:** <exception> when <condition>

**Example:**
<runnable PHP snippet>
```

### Conventions

- Return types use PHP type-hint syntax: `NDArray`, `array`, `int`, `float`, `string`, `bool`, `void`, `mixed`.
- Parameter types follow the same syntax. `null` defaults are written as `?Type` where appropriate.
- Static methods are marked with `public static function`. Instance methods omit `static`.
- The `dtype` parameter is always one of `"float32"`, `"float64"`, `"int32"`, `"int64"` (per [decision 2](../system.md)). Default is `"float64"` everywhere it appears unless noted.
- Examples are runnable as-is with `php -dextension=numphp.so`. Output shown below the snippet is what the script actually prints — verified before this reference was committed.
- Where a method's behavior depends on a system-level decision (dtype promotion, NaN policy, round-half), the entry links to the relevant decision number in [`docs/system.md`](../system.md).

### Naming

- The extension is referred to as `numphp` (lowercase) in code, install commands, and CLI contexts; as `NumPHP` (mixed case) in prose. See [decision 26](../system.md).
- Exceptions live in the global namespace (`\NDArrayException`, etc.) rather than under `numphp\` — this matches the PHP core convention for `\RuntimeException`-derived exceptions thrown from extensions.

## Concept guides

For background on cross-cutting behaviour — dtype promotion, broadcasting, view-vs-copy, NaN policy, round-half — see [`docs/concepts/`](../concepts/).

## Cheat sheet

A side-by-side NumPy ↔ NumPHP table for the most common operations: [`docs/cheatsheet-numpy.md`](../cheatsheet-numpy.md).
