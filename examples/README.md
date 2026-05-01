# NumPHP examples

Five runnable scripts demonstrating canonical workflows. Each is
self-contained, deterministic, and has a checked-in `.expected` file holding
the exact stdout it produces — CI diffs the two so any drift breaks the
build.

## Scripts

| File | Demonstrates |
|------|--------------|
| [`linear-regression.php`](linear-regression.php) | Ordinary least squares via `Linalg::solve` on the normal equations `(XᵀX) β = Xᵀy`. |
| [`kmeans.php`](kmeans.php) | Lloyd's k-means with broadcasting (distance via reshape + subtract), `argmin` for assignment, view-vs-copy for centroid update. |
| [`image-as-array.php`](image-as-array.php) | Treats a 2-D NDArray as a grayscale image: gradient → threshold → 4×4 view → P5 PGM round-trip. |
| [`time-series.php`](time-series.php) | 1-D series, 7-day rolling mean via `slice + mean`, view-mutation visibility, `nanmean` over injected holes. |
| [`csv-pipeline.php`](csv-pipeline.php) | `fromCsv` → axis-wise reductions with `keepdims` → broadcasted normalisation → `save` / `load` round-trip. |

## Run one

```bash
php -d extension=./modules/numphp.so examples/linear-regression.php
```

## Run all

```bash
for f in examples/*.php; do
    diff <(php -d extension=./modules/numphp.so "$f") "${f%.php}.expected"
done && echo all-green
```

## Data fixtures

`data/iris.csv` is a 15-row slice of the UCI Iris dataset (public domain).
See [`data/README.md`](data/README.md) for provenance.

No example writes outside `examples/data/` or `sys_get_temp_dir()`. Anything
written to `/tmp` is unlinked before the script returns.
