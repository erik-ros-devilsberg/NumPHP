--TEST--
var / std — Welford's algorithm, ddof, axis
--FILE--
<?php
$a = NDArray::fromArray([2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0]);

// Population variance (ddof=0): 4.0
$v = $a->var();
echo abs($v - 4.0) < 1e-12 ? "var-ok\n" : "var-fail: $v\n";

// Population std: 2.0
$s = $a->std();
echo abs($s - 2.0) < 1e-12 ? "std-ok\n" : "std-fail: $s\n";

// Sample variance (ddof=1): 32 / 7
$sv = $a->var(null, false, 1);
echo abs($sv - 32.0/7.0) < 1e-12 ? "samplevar-ok\n" : "samplevar-fail: $sv\n";

// Welford precision check: large mean, small spread.
// Naive E[x^2] - E[x]^2 catastrophically cancels here. Welford should be near-zero.
$big = [];
for ($i = 0; $i < 100; $i++) $big[] = 1e9 + $i;
$bn = NDArray::fromArray($big);
// Population variance of 0..99 is 833.25
$bv = $bn->var();
echo abs($bv - 833.25) < 1e-6 ? "welford-stable\n" : "welford-unstable: $bv\n";

// Axis reductions
$m = NDArray::fromArray([[1.0, 2.0], [3.0, 4.0], [5.0, 6.0]]);
$v0 = $m->var(0);
var_dump($v0->shape());                           // [2]
echo abs($v0[0] - 8.0/3.0) < 1e-12 ? "axis0-var-ok\n" : "axis0-var-fail\n";
echo abs($v0[1] - 8.0/3.0) < 1e-12 ? "axis0-var2-ok\n" : "axis0-var2-fail\n";

$v1 = $m->var(1);
var_dump($v1->toArray());                         // [0.25, 0.25, 0.25]

// Integer input → f64 output
$ai = NDArray::fromArray([1, 2, 3, 4, 5]);
$vi = $ai->var();
var_dump(is_float($vi));                          // true
echo abs($vi - 2.0) < 1e-12 ? "int-var-ok\n" : "int-var-fail: $vi\n";

// Axis var on int input → f64 array
var_dump(NDArray::fromArray([[1, 2], [3, 4]])->var(0)->dtype());  // float64

// NaN propagates by default
$nan = NDArray::fromArray([1.0, NAN, 3.0]);
echo is_nan($nan->var()) ? "var-nan-ok\n" : "var-nan-fail\n";
?>
--EXPECT--
var-ok
std-ok
samplevar-ok
welford-stable
array(1) {
  [0]=>
  int(2)
}
axis0-var-ok
axis0-var2-ok
array(3) {
  [0]=>
  float(0.25)
  [1]=>
  float(0.25)
  [2]=>
  float(0.25)
}
bool(true)
int-var-ok
string(7) "float64"
var-nan-ok
