<?php
/**
 * numphp benchmark runner.
 *
 * Reads bench/scenarios.json, runs every scenario against the live
 * extension, prints one JSON object per scenario to stdout (one per
 * line) so a downstream comparator can ingest it.
 *
 * Methodology (per scenarios.json["methodology"]):
 *   - one untimed warmup run per scenario
 *   - 7 timed runs
 *   - drop the slowest 1
 *   - report median, min, max of the remaining 6
 *
 * Per-scenario timings cover the operation only — fixture allocation
 * (and any pre-computation that isn't the subject) happens before
 * the timer starts.
 *
 * Usage:
 *   php -d extension=./modules/numphp.so bench/run.php           # full suite
 *   php -d extension=./modules/numphp.so bench/run.php --tiny    # smoke (one tiny scenario)
 */

declare(strict_types=1);

$tiny = in_array('--tiny', $argv, true);

$root = dirname(__DIR__);
$cfg = json_decode(file_get_contents(__DIR__ . '/scenarios.json'), true, flags: JSON_THROW_ON_ERROR);
$method = $cfg['methodology'];
$scenarios = $cfg['scenarios'];

if ($tiny) {
    $scenarios = [[
        'id' => 'elementwise-add-100x100-f64',
        'label' => 'smoke',
        'shape' => [100, 100],
        'dtype' => 'float64',
        'category' => 'elementwise',
        'description' => 'tiny smoke run',
    ]];
    $method['runs'] = 2;
    $method['drop_slowest'] = 0;
    $method['warmup_runs'] = 1;
}

mt_srand($method['rng_seed']);

/**
 * Build a deterministic-but-non-trivial fixture array of the requested
 * shape and dtype. Avoids using NDArray::fromArray for the source data
 * (we measure that path separately) — instead use arange + reshape +
 * cast, which is BLAS-fast.
 */
function fixture(array $shape, string $dtype): NDArray {
    $size = (int) array_product($shape);
    return NDArray::arange(0, $size, 1, $dtype)->reshape($shape);
}

function fixture_php_array(array $shape): array {
    [$r, $c] = $shape;
    $out = [];
    for ($i = 0; $i < $r; $i++) {
        $row = [];
        for ($j = 0; $j < $c; $j++) $row[] = $i * $c + $j + 0.5;
        $out[] = $row;
    }
    return $out;
}

function ns(): int { return hrtime(true); }

/**
 * Run a callable `runs` times, drop slowest `drop` runs, return
 * median/min/max in nanoseconds across the survivors.
 */
function measure(callable $op, int $warmup, int $runs, int $drop): array {
    for ($i = 0; $i < $warmup; $i++) $op();
    $samples = [];
    for ($i = 0; $i < $runs; $i++) {
        $t0 = ns();
        $op();
        $samples[] = ns() - $t0;
    }
    sort($samples);
    if ($drop > 0) array_splice($samples, count($samples) - $drop);
    $n = count($samples);
    $median = $n % 2
        ? $samples[intdiv($n, 2)]
        : (int) (($samples[intdiv($n, 2) - 1] + $samples[intdiv($n, 2)]) / 2);
    return [
        'median_ns' => $median,
        'min_ns'    => $samples[0],
        'max_ns'    => $samples[$n - 1],
        'samples'   => $samples,
    ];
}

/**
 * Dispatch each scenario id to the matching benchmarked closure. Each
 * closure receives `$shape` and `$dtype` and returns the operation
 * to time (variables it needs already in scope).
 */
function build_op(array $s): callable {
    $shape = $s['shape']; $dtype = $s['dtype']; $id = $s['id'];

    return match (true) {
        str_starts_with($id, 'elementwise-add-') => (function () use ($shape, $dtype) {
            $a = fixture($shape, $dtype); $b = fixture($shape, $dtype);
            return fn() => NDArray::add($a, $b);
        })(),
        str_starts_with($id, 'elementwise-mul-') => (function () use ($shape, $dtype) {
            $a = fixture($shape, $dtype); $b = fixture($shape, $dtype);
            return fn() => NDArray::multiply($a, $b);
        })(),
        str_starts_with($id, 'matmul-') => (function () use ($shape, $dtype) {
            $a = fixture($shape, $dtype); $b = fixture($shape, $dtype);
            return fn() => NDArray::matmul($a, $b);
        })(),
        str_starts_with($id, 'sum-axis0-') => (function () use ($shape, $dtype) {
            $a = fixture($shape, $dtype);
            return fn() => $a->sum(0);
        })(),
        str_starts_with($id, 'sum-axis1-') => (function () use ($shape, $dtype) {
            $a = fixture($shape, $dtype);
            return fn() => $a->sum(1);
        })(),
        str_starts_with($id, 'fromArray-') => (function () use ($shape, $dtype) {
            $php = fixture_php_array($shape);
            return fn() => NDArray::fromArray($php, $dtype);
        })(),
        str_starts_with($id, 'toArray-') => (function () use ($shape, $dtype) {
            $a = fixture($shape, $dtype);
            return fn() => $a->toArray();
        })(),
        str_starts_with($id, 'linalg-solve-') => (function () use ($shape, $dtype) {
            // make A non-singular by adding a diagonal weight
            $A = NDArray::add(fixture($shape, $dtype), NDArray::eye($shape[0], null, 0, $dtype));
            $b = fixture([$shape[0]], $dtype);
            return fn() => Linalg::solve($A, $b);
        })(),
        str_starts_with($id, 'linalg-inv-') => (function () use ($shape, $dtype) {
            $A = NDArray::add(fixture($shape, $dtype), NDArray::eye($shape[0], null, 0, $dtype));
            return fn() => Linalg::inv($A);
        })(),
        str_starts_with($id, 'slice-view-') => (function () use ($shape, $dtype) {
            $a = fixture($shape, $dtype);
            return fn() => $a->slice(500, 4500);
        })(),
        default => throw new RuntimeException("no op binding for scenario {$s['id']}"),
    };
}

foreach ($scenarios as $s) {
    $op = build_op($s);
    $res = measure($op, $method['warmup_runs'], $method['runs'], $method['drop_slowest']);
    echo json_encode([
        'engine'      => 'numphp',
        'id'          => $s['id'],
        'label'       => $s['label'],
        'shape'       => $s['shape'],
        'dtype'       => $s['dtype'],
        'category'    => $s['category'],
        'description' => $s['description'],
        'median_ns'   => $res['median_ns'],
        'min_ns'      => $res['min_ns'],
        'max_ns'      => $res['max_ns'],
        'samples_ns'  => $res['samples'],
    ]), "\n";
}
