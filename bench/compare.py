"""
Combine numphp + numpy run output into a Markdown table.

Reads two JSONL files (one object per line, schema as emitted by
run.php and run.py), joins on `id`, prints a Markdown table to stdout.

Usage:
  python bench/compare.py numphp.jsonl numpy.jsonl
"""

from __future__ import annotations

import json
import sys
from pathlib import Path


def load(path):
    by_id = {}
    for line in Path(path).read_text().splitlines():
        line = line.strip()
        if not line:
            continue
        rec = json.loads(line)
        by_id[rec["id"]] = rec
    return by_id


def fmt_ms(ns):
    return f"{ns / 1e6:,.3f}"


def fmt_us(ns):
    return f"{ns / 1e3:,.1f}"


def main():
    if len(sys.argv) != 3:
        print("usage: python compare.py numphp.jsonl numpy.jsonl", file=sys.stderr)
        sys.exit(2)

    nph = load(sys.argv[1])
    nmp = load(sys.argv[2])
    ids = list(nph.keys())  # PHP run drives the order

    print("| Scenario | Shape | dtype | numphp (ms) | NumPy (ms) | ratio (numphp / numpy) |")
    print("|----------|-------|-------|-------------|------------|-----------------------|")
    for sid in ids:
        a = nph[sid]
        b = nmp.get(sid)
        shape = "x".join(str(s) for s in a["shape"])
        if b is None:
            print(
                f"| {a['label']} | {shape} | {a['dtype']} | "
                f"{fmt_ms(a['median_ns'])} | _missing_ | — |"
            )
            continue
        ratio = a["median_ns"] / b["median_ns"] if b["median_ns"] > 0 else float("inf")
        # Sub-millisecond scenarios benefit from microsecond display.
        if max(a["median_ns"], b["median_ns"]) < 1e6:
            php_str = f"{fmt_us(a['median_ns'])} µs"
            np_str = f"{fmt_us(b['median_ns'])} µs"
        else:
            php_str = fmt_ms(a["median_ns"])
            np_str = fmt_ms(b["median_ns"])
        print(
            f"| {a['label']} | {shape} | {a['dtype']} | "
            f"{php_str} | {np_str} | {ratio:.2f}× |"
        )


if __name__ == "__main__":
    main()
