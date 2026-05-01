# numphp benchmarks

Compares numphp against NumPy on the same hardware, same BLAS, same
seed. Output is reproducible — re-run on the same machine and the
numbers should be within noise.

## Run

```bash
bash bench/run.sh                 # full suite, ~10s on a Zenbook
bash bench/run.sh --tiny          # smoke, one tiny scenario each side
```

First invocation creates `bench/.venv/` (gitignored) and
`pip install`s NumPy into it. The system Python is not touched.

## Files

| | |
|---|---|
| `scenarios.json` | Single source of truth — both runners read this. |
| `run.php` | numphp runner. Emits one JSON object per scenario. |
| `run.py` | NumPy runner. Mirror image of `run.php`. |
| `compare.py` | Joins the two JSONL streams into a Markdown table. |
| `fingerprint.sh` | Captures hardware/BLAS/version metadata. |
| `run.sh` | Orchestrates the above; emits a full Markdown report. |
| `last-run/` | Intermediate JSONL + fingerprint from the most recent run (gitignored). |

## Methodology

Locked as decision 30 in `docs/system.md`. Summary:

- 7 timed runs per scenario, drop slowest 1, report median + min + max.
- One untimed warmup run per scenario.
- `mt_srand(42)` / `np.random.seed(42)`. Identical synthetic data.
- `hrtime(true)` (PHP) / `time.perf_counter_ns()` (Python). Both
  monotonic, nanosecond-precision.
- Per-scenario timing covers the operation only — fixture allocation
  is excluded except where the fixture *is* the subject (`fromArray`,
  `toArray`).

## Adding a scenario

Edit `scenarios.json`. If the new id matches an existing prefix
(`elementwise-add-`, `matmul-`, `sum-axis0-`, etc.) the existing
dispatch in `run.php::build_op` and `run.py::build_op` handles it.
Otherwise add a new branch in both runners.

## Honest weak spots

This is a deliberately honest benchmark. We publish what we lose
alongside what we win. As of `0.0.13`:

- **Element-wise ops** (~2.5× slower than NumPy) — our nd-iterator is
  generic; NumPy's inner loops are SIMD-vectorised.
- **`sum` along axis 0** (~15× slower) — non-stride-1 reduction;
  cache-unfriendly direction; we don't yet have a vectorised
  per-axis kernel.
- **`sum` along axis 1** (~4× slower) — better than axis 0 because
  it's contiguous, but NumPy still wins.

## Where we win

- **`matmul` (f64 and f32)** — parity (~1.0×). Both engines call the
  same OpenBLAS routine.
- **`Linalg::solve` / `Linalg::inv`** — parity to ~0.6×. Same LAPACK
  underneath; our F-contig copy doesn't dominate.
- **`fromArray` / `toArray`** — ~0.35× (faster than NumPy). PHP's
  array traversal beats Python's list-of-list iteration on 1M
  elements.
- **`slice` (view creation)** — sub-microsecond on both sides; our
  pointer arithmetic edges out NumPy's.
