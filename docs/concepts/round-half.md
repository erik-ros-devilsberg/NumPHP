# Round-Half Semantics

`NDArray::round()` rounds **half-away-from-zero**. NumPy's `np.round` rounds half-to-even (banker's rounding). This is a deliberate divergence — one of the few places where NumPHP departs from NumPy. It is locked by [decision 8](../system.md) and by `tests/036-elementwise-math.phpt`.

This page documents the choice, the consequences, and the workaround for users who need NumPy's behaviour.

---

## The two conventions

For most inputs, both rules agree: `1.49 → 1`, `1.51 → 2`, `-2.7 → -3`. They differ only when the input is exactly halfway between two integers.

| Value | NumPy (banker's) | NumPHP / PHP `round()` |
|-------|------------------|-------------------------|
| `0.5` | `0.0` | `1.0` |
| `1.5` | `2.0` | `2.0` |
| `2.5` | `2.0` | `3.0` |
| `3.5` | `4.0` | `4.0` |
| `-0.5` | `-0.0` | `-1.0` |
| `-1.5` | `-2.0` | `-2.0` |
| `-2.5` | `-2.0` | `-3.0` |

NumPy's rule is symmetric: ties go to the nearest even integer. Half the time you round up, half the time you round down — over a uniform distribution of halfway-ties, the bias cancels out. This is why it is the conventional choice in numerical analysis.

NumPHP's rule is the one PHP's scalar `round()` uses (with mode `PHP_ROUND_HALF_UP`): ties always move away from zero. It introduces a small bias on tied inputs, but it matches the surface-level intuition most PHP users have.

---

## Why NumPHP picked the PHP convention

The PHP standard library's `round()` rounds half-away-from-zero. PHP code that mixes scalar `round()` and `NDArray::round()` would produce inconsistent results if NumPHP picked the NumPy rule:

```php
echo round(0.5), "\n";                                  // 1   (PHP)
echo NDArray::fromArray([0.5])->round()->toArray()[0];  // would be 0 if we matched NumPy
```

The cost of choosing differently from NumPy is paid once at documentation time and reminded by the cheatsheet. The cost of matching NumPy would be paid on every PHP test, every port from existing PHP code, and every "I rounded a number and got something I didn't expect" support thread. The first cost is bounded; the second is unbounded.

The decision is locked by `tests/036-elementwise-math.phpt`, which asserts the exact rounding behaviour on the seven cases in the table above. Any future change to banker's rounding will fail loudly — the test exists specifically to make accidental drift impossible.

---

## Worked example

```php
$x = NDArray::fromArray([0.5, 1.5, 2.5, 3.5, -0.5, -1.5, -2.5]);
print_r($x->round()->toArray());
```

```
Array ( [0] => 1 [1] => 2 [2] => 3 [3] => 4 [4] => -1 [5] => -2 [6] => -3 )
```

Compare to the NumPy column above — every halfway value goes a different way.

---

## Decimal places

`round()` accepts a `$decimals` argument (default `0`):

```php
$x = NDArray::fromArray([1.234, 1.235, 1.245]);
print_r($x->round(2)->toArray());
```

```
Array ( [0] => 1.23 [1] => 1.24 [2] => 1.25 )
```

The half-away-from-zero rule applies at the chosen decimal place. Negative `$decimals` rounds to powers of ten: `round(-2)` rounds to the nearest 100.

---

## Workaround: NumPy-equivalent banker's rounding

If you genuinely need NumPy's banker's rounding (e.g. to match a Python implementation bit-for-bit), implement it in PHP atop `floor`/`ceil`:

```php
/** Banker's rounding (half-to-even) — NumPy-compatible. */
function np_round(NDArray $x, int $decimals = 0): NDArray {
    $scale = 10 ** $decimals;
    $scaled = $x->multiply($x, NDArray::full($x->shape(), $scale, $x->dtype()));
    $down = $scaled->floor();
    $up   = $scaled->ceil();
    $halfway = $scaled->subtract($scaled, $down)->add(... /* etc. */);
    // … pick down/up based on parity of $down …
}
```

In practice most users will not need this — the agreement on every non-halfway input is already complete, and the halfway divergence rarely affects results outside of pathological test fixtures.

---

## See also

- [Decision 8](../system.md) — the choice and rationale.
- `tests/036-elementwise-math.phpt` — the lock test.
- [`NDArray::round`](../api/ndarray.md#ndarrayround-ndarray) — API reference entry.
