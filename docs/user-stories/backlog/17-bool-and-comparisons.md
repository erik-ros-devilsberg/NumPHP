---
story: Bool dtype, comparison operators, and `where`
created: 2026-05-01
---

> Part of [Epic: NumPHP](epic-numphp.md)

## Description

The most common pattern in NumPy code is a comparison producing a
boolean array, then either using that array as a mask or feeding it
to `np.where`:

```python
arr > 5               # → bool array
np.where(x > 0, x, 0) # → relu (clip negatives to zero)
```

numphp can't currently do either: there is no `bool` dtype (decision
2 deferred it), no comparison ops returning arrays, and no `where`.
A user porting a NumPy snippet hits one of these on roughly every
other line and stops.

This story adds:

1. **`bool` dtype** — 1 byte per element, stored as 0/1.
2. **Six comparison ops** as `NDArray::eq($a, $b)`, `::ne`, `::lt`,
   `::le`, `::gt`, `::ge`, each returning a fresh `bool` NDArray.
   Broadcasting and dtype promotion work as in arithmetic.
3. **`NDArray::where(cond, x, y)`** — element-select. `cond` is a
   bool NDArray; `x` and `y` are scalars or NDArrays broadcastable
   with each other and with `cond`. Output dtype is the promotion
   of `x` and `y`.

PHP's `==` / `!=` / `<` / `>` / `<=` / `>=` operators are **not**
overloaded for NDArray in v0 — Zend Engine doesn't expose comparison
overload as cleanly as arithmetic, and using a method API
(`$a->gt($b)`) keeps the syntax explicit. Revisit if a clean
overload path appears.

## Acceptance Criteria

- `bool` is a recognised dtype string in every factory: `zeros`,
  `ones`, `full`, `eye`, `arange`, `fromArray`, plus `save`/`load`
  binary format.
- bool storage uses 1 byte per element. `fromArray` accepts native
  PHP `true`/`false` leaves; integer or float leaves cast via the
  usual rules (0 → false, anything else → true).
- bool participates in dtype promotion: `bool + bool = bool`;
  `bool + int{32,64} = int{32,64}`; `bool + float{32,64} =
  float{32,64}`.
- Six comparison methods on NDArray (`eq`, `ne`, `lt`, `le`, `gt`,
  `ge`), each `public static`, accepting two operands (NDArray or
  scalar), returning an NDArray of dtype `bool` with the broadcast
  shape. Mixed-dtype operands promote before comparing.
- `NDArray::where($cond, $x, $y)` — `cond` is a bool NDArray;
  `x`/`y` are scalars or NDArrays broadcastable with `cond` and
  each other. Output shape is the broadcast of all three; output
  dtype is the promotion of `x` and `y` (cond's dtype is irrelevant
  to output dtype).
- NaN handling in comparisons: any NaN operand makes `eq` / `ne` /
  `lt` / `le` / `gt` / `ge` return false (matches IEEE 754, matches
  NumPy).
- Documentation added for the new dtype + ops in `docs/api/ndarray.md`
  and `docs/concepts/dtypes.md`. Cheatsheet entry added.
- New phpt tests cover the dtype, each comparison op, broadcasting,
  promotion, NaN cases, and `where`.

## Out of scope

- **Boolean indexing** (`$arr[$cond]` returning the elements where
  cond is true). Bigger sprint that depends on this one — picks up
  after.
- **`any` / `all` reductions** on bool arrays. Small follow-up
  sprint; can ship together with boolean indexing or alone.
- **Comparison-operator overloading** for `==` etc. — Zend Engine
  hooks for comparison are awkward; punt until / unless someone
  finds a clean path.
- **Bitwise ops on bool** (`&`, `|`, `^`, `~` for elementwise AND /
  OR / XOR / NOT). Useful for combining masks. Not in this sprint;
  pairs naturally with boolean indexing in a follow-up.
- **`bool` as a parameter to existing reductions** (e.g.
  `arr->sum()` on a bool array returning the count of true). Works
  for free if dtype promotion does its job; testing it is in scope,
  no new code.
