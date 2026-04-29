--TEST--
sum / mean — global, axis, keepdims, dtype rules
--FILE--
<?php
$a = NDArray::fromArray([[1, 2, 3], [4, 5, 6]]);

// Global
var_dump($a->sum());                              // int(21)
var_dump($a->mean());                             // float(3.5)

// Axis 0
var_dump($a->sum(0)->toArray());                  // [5, 7, 9]
var_dump($a->mean(0)->toArray());                 // [2.5, 3.5, 4.5]

// Axis 1
var_dump($a->sum(1)->toArray());                  // [6, 15]
var_dump($a->mean(1)->toArray());                 // [2.0, 5.0]

// keepdims
var_dump($a->sum(0, true)->shape());              // [1, 3]
var_dump($a->sum(1, true)->shape());              // [2, 1]
var_dump($a->mean(null, true)->shape());          // [1, 1]

// f32 input keeps f32 sum dtype
$f32 = NDArray::full([2, 2], 1.5, 'float32');
var_dump($f32->sum(0)->dtype());                  // float32
var_dump($f32->mean(0)->dtype());                 // float32 (mean preserves f32)

// int sum/mean dtype
var_dump($a->sum(0)->dtype());                    // int64
var_dump($a->mean(0)->dtype());                   // float64

// Pairwise sum precision: 1000 copies of 0.1 should equal 100.0 within tight tolerance.
// (Naive accumulation drifts by ~5e-13; pairwise is bit-perfect on this case.)
$arr = array_fill(0, 1000, 0.1);
$big = NDArray::fromArray($arr);
$s = $big->sum();
echo abs($s - 100.0) < 1e-12 ? "pairwise-ok\n" : "pairwise-fail: $s\n";

// Negative axis
var_dump($a->sum(-1)->toArray());                 // [6, 15]
var_dump($a->sum(-2)->toArray());                 // [5, 7, 9]

// Out-of-range axis
try { $a->sum(2); } catch (ShapeException $e) { echo "axis-oor: ", $e->getMessage(), "\n"; }
?>
--EXPECT--
int(21)
float(3.5)
array(3) {
  [0]=>
  int(5)
  [1]=>
  int(7)
  [2]=>
  int(9)
}
array(3) {
  [0]=>
  float(2.5)
  [1]=>
  float(3.5)
  [2]=>
  float(4.5)
}
array(2) {
  [0]=>
  int(6)
  [1]=>
  int(15)
}
array(2) {
  [0]=>
  float(2)
  [1]=>
  float(5)
}
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(3)
}
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(1)
}
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(1)
}
string(7) "float32"
string(7) "float32"
string(5) "int64"
string(7) "float64"
pairwise-ok
array(2) {
  [0]=>
  int(6)
  [1]=>
  int(15)
}
array(3) {
  [0]=>
  int(5)
  [1]=>
  int(7)
  [2]=>
  int(9)
}
axis-oor: axis 2 out of range for ndim 2
