"""
NumPy benchmark runner — mirror image of bench/run.php.

Reads bench/scenarios.json, runs every scenario via NumPy, prints
one JSON object per scenario to stdout (one per line) so a downstream
comparator can ingest both runs side-by-side.

Methodology must match run.php exactly:
  - one untimed warmup run per scenario
  - 7 timed runs
  - drop the slowest 1
  - report median, min, max of the remaining 6

Per-scenario timings cover the operation only — fixture allocation
happens before the timer starts.

Usage (via the venv):
  bench/.venv/bin/python bench/run.py            # full suite
  bench/.venv/bin/python bench/run.py --tiny     # smoke
"""

from __future__ import annotations

import argparse
import json
import statistics
import sys
import time
from pathlib import Path

import numpy as np


def fixture(shape, dtype):
    """Match run.php::fixture — arange reshape, deterministic."""
    size = int(np.prod(shape))
    return np.arange(0, size, 1, dtype=dtype).reshape(shape)


def fixture_python_list(shape):
    """Match run.php::fixture_php_array — nested list of floats."""
    r, c = shape
    return [[i * c + j + 0.5 for j in range(c)] for i in range(r)]


def ns():
    return time.perf_counter_ns()


def measure(op, warmup, runs, drop):
    for _ in range(warmup):
        op()
    samples = []
    for _ in range(runs):
        t0 = ns()
        op()
        samples.append(ns() - t0)
    samples.sort()
    if drop > 0:
        samples = samples[: len(samples) - drop]
    median = statistics.median(samples)
    return {
        "median_ns": int(median),
        "min_ns": samples[0],
        "max_ns": samples[-1],
        "samples": samples,
    }


def build_op(s):
    """Return a zero-arg callable that performs the operation under test."""
    shape = tuple(s["shape"])
    dtype = np.float32 if s["dtype"] == "float32" else np.float64
    sid = s["id"]

    if sid.startswith("elementwise-add-"):
        a = fixture(shape, dtype)
        b = fixture(shape, dtype)
        return lambda: a + b
    if sid.startswith("elementwise-mul-"):
        a = fixture(shape, dtype)
        b = fixture(shape, dtype)
        return lambda: a * b
    if sid.startswith("matmul-"):
        a = fixture(shape, dtype)
        b = fixture(shape, dtype)
        return lambda: a @ b
    if sid.startswith("sum-axis0-"):
        a = fixture(shape, dtype)
        return lambda: a.sum(axis=0)
    if sid.startswith("sum-axis1-"):
        a = fixture(shape, dtype)
        return lambda: a.sum(axis=1)
    if sid.startswith("fromArray-"):
        py = fixture_python_list(shape)
        return lambda: np.array(py, dtype=dtype)
    if sid.startswith("toArray-"):
        a = fixture(shape, dtype)
        return lambda: a.tolist()
    if sid.startswith("linalg-solve-"):
        a = fixture(shape, dtype) + np.eye(shape[0], dtype=dtype)
        b = fixture((shape[0],), dtype)
        return lambda: np.linalg.solve(a, b)
    if sid.startswith("linalg-inv-"):
        a = fixture(shape, dtype) + np.eye(shape[0], dtype=dtype)
        return lambda: np.linalg.inv(a)
    if sid.startswith("slice-view-"):
        a = fixture(shape, dtype)
        return lambda: a[500:4500]
    raise RuntimeError(f"no op binding for scenario {sid}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tiny", action="store_true")
    args = ap.parse_args()

    cfg = json.loads((Path(__file__).parent / "scenarios.json").read_text())
    method = cfg["methodology"]
    scenarios = cfg["scenarios"]

    if args.tiny:
        scenarios = [{
            "id": "elementwise-add-100x100-f64",
            "label": "smoke",
            "shape": [100, 100],
            "dtype": "float64",
            "category": "elementwise",
            "description": "tiny smoke run",
        }]
        method = {"runs": 2, "drop_slowest": 0, "warmup_runs": 1, "rng_seed": 42}

    np.random.seed(method["rng_seed"])

    for s in scenarios:
        op = build_op(s)
        res = measure(op, method["warmup_runs"], method["runs"], method["drop_slowest"])
        print(json.dumps({
            "engine": "numpy",
            "id": s["id"],
            "label": s["label"],
            "shape": s["shape"],
            "dtype": s["dtype"],
            "category": s["category"],
            "description": s["description"],
            "median_ns": res["median_ns"],
            "min_ns": res["min_ns"],
            "max_ns": res["max_ns"],
            "samples_ns": res["samples"],
        }))


if __name__ == "__main__":
    sys.exit(main())
