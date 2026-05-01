--TEST--
±inf propagates correctly through every full reduction
--FILE--
<?php
$pos_inf = NDArray::fromArray([1.0, INF, 2.0]);
$neg_inf = NDArray::fromArray([1.0, -INF, 2.0]);
$mixed   = NDArray::fromArray([INF, -INF, 0.0]);

// sum: +inf wins; mixed +∞/-∞ → NaN (IEEE 754).
echo "sum(+inf):   ", $pos_inf->sum(), "\n";        // INF
echo "sum(-inf):   ", $neg_inf->sum(), "\n";        // -INF
echo "sum(mixed): ", is_nan($mixed->sum()) ? "NaN" : "FAIL", "\n";

// mean: +inf wins; mixed → NaN.
echo "mean(+inf):  ", $pos_inf->mean(), "\n";       // INF
echo "mean(-inf):  ", $neg_inf->mean(), "\n";       // -INF
echo "mean(mixed): ", is_nan($mixed->mean()) ? "NaN" : "FAIL", "\n";

// min / max: see the extremes.
echo "min(+inf):  ", $pos_inf->min(), "\n";         // 1
echo "max(+inf):  ", $pos_inf->max(), "\n";         // INF
echo "min(-inf):  ", $neg_inf->min(), "\n";         // -INF
echo "max(-inf):  ", $neg_inf->max(), "\n";         // 2

// argmin / argmax point at the extreme.
echo "argmax(+inf): ", $pos_inf->argmax(), "\n";    // 1
echo "argmin(-inf): ", $neg_inf->argmin(), "\n";    // 1

// var / std: variance over a set containing inf is NaN or inf depending
// on definition; numphp uses Welford which produces NaN for inf input
// (centered differences include inf - inf).
echo "var(+inf):  ", is_nan($pos_inf->var()) || is_infinite($pos_inf->var()) ? "non-finite" : "FAIL", "\n";
echo "std(+inf):  ", is_nan($pos_inf->std()) || is_infinite($pos_inf->std()) ? "non-finite" : "FAIL", "\n";

// Axis reduction: ±inf on the reduction axis still propagates correctly.
$m = NDArray::fromArray([[1.0, INF], [2.0, 3.0]]);
$col_max = $m->max(0)->toArray();
$labels = array_map(fn($v) => is_infinite($v) ? ($v > 0 ? "INF" : "-INF") : (string)$v, $col_max);
echo "max over axis 0 with inf in col 1: [", implode(',', $labels), "]\n";  // [2, INF]

// nan-aware variants don't touch inf — inf is finite-arithmetic, not NaN.
echo "nansum(+inf): ", $pos_inf->nansum(), "\n";    // INF
echo "nanmean(-inf): ", $neg_inf->nanmean(), "\n";  // -INF
?>
--EXPECT--
sum(+inf):   INF
sum(-inf):   -INF
sum(mixed): NaN
mean(+inf):  INF
mean(-inf):  -INF
mean(mixed): NaN
min(+inf):  1
max(+inf):  INF
min(-inf):  -INF
max(-inf):  2
argmax(+inf): 1
argmin(-inf): 1
var(+inf):  non-finite
std(+inf):  non-finite
max over axis 0 with inf in col 1: [2,INF]
nansum(+inf): INF
nanmean(-inf): -INF
