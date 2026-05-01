<?php
/**
 * End-to-end CSV → analyse → save pipeline.
 *
 * Reads a 15-row, 4-column CSV (a small slice of the Iris dataset, header
 * skipped), computes per-column mean and standard deviation, normalises the
 * data to z-scores, and writes the normalised array to NumPHP binary format.
 * Then loads the binary back and confirms the values round-trip.
 *
 * Demonstrates: NDArray::fromCsv, axis-wise reductions with broadcasting,
 * NDArray::save / NDArray::load, dtype preservation across I/O.
 */

$csv_path = __DIR__ . '/data/iris.csv';
$data = NDArray::fromCsv($csv_path, 'float64', /* header = */ true);
echo "loaded ", $data->shape()[0], " rows × ", $data->shape()[1], " cols\n";

// Per-column reductions: axis=0 reduces the rows, leaving a (1, ncols)
// array (with keepdims=true). keepdims keeps it broadcastable for the
// normalisation step.
$mean = $data->mean(0, /* keepdims = */ true);
$std  = $data->std(0,  /* keepdims = */ true);

echo "column means: ";
foreach ($mean->toArray()[0] as $v) printf("%.4f ", $v);
echo "\n";
echo "column stds:  ";
foreach ($std->toArray()[0]  as $v) printf("%.4f ", $v);
echo "\n";

// Normalise: (x - μ) / σ. Broadcasting works because mean and std are
// (1, 4) and data is (15, 4).
$z = NDArray::divide(NDArray::subtract($data, $mean), $std);

// Sanity check: per-column mean of z should be ~0, per-column std ~1.
$z_means = $z->mean(0, false)->toArray();
$z_stds  = $z->std(0,  false)->toArray();
$max_mean_dev = max(array_map('abs', $z_means));
$max_std_dev  = max(array_map(fn($s) => abs($s - 1.0), $z_stds));
printf("max |z-column-mean|     = %.2e\n", $max_mean_dev);
printf("max |z-column-std - 1|  = %.2e\n", $max_std_dev);

// Save to NumPHP binary format and round-trip.
$out = sys_get_temp_dir() . '/iris-z.npp';
$z->save($out);
$reloaded = NDArray::load($out);
unlink($out);

echo "reloaded shape: ", implode('x', $reloaded->shape()), "\n";
echo "reloaded dtype: ", $reloaded->dtype(), "\n";

$diff = NDArray::subtract($z, $reloaded)->abs()->max();
printf("max |z - reload|        = %.2e\n", $diff);
echo "OK\n";
