--TEST--
Element-wise math: sqrt, exp, log, abs, power, clip, floor, ceil, round
--FILE--
<?php
// sqrt
$a = NDArray::fromArray([[1.0, 4.0], [9.0, 16.0]]);
var_dump($a->sqrt()->toArray());                           // [[1.0, 2.0], [3.0, 4.0]]

// sqrt on int → f64
$ai = NDArray::fromArray([1, 4, 9, 16]);
var_dump($ai->sqrt()->dtype());                            // float64

// f32 input → f32 sqrt
$f32 = NDArray::full([2], 4.0, 'float32');
var_dump($f32->sqrt()->dtype());                           // float32

// exp / log / log2 / log10 → always f64
$e = NDArray::fromArray([0.0, 1.0]);
var_dump($e->exp()->dtype());                              // float64
$exp_arr = $e->exp()->toArray();
echo abs($exp_arr[0] - 1.0) < 1e-12 ? "exp0-ok\n" : "exp0-fail\n";
echo abs($exp_arr[1] - M_E) < 1e-12 ? "exp1-ok\n" : "exp1-fail\n";

$l = NDArray::fromArray([1.0, M_E]);
$larr = $l->log()->toArray();
echo abs($larr[0]) < 1e-12 ? "log0-ok\n" : "log0-fail\n";
echo abs($larr[1] - 1.0) < 1e-12 ? "log1-ok\n" : "log1-fail\n";

var_dump(NDArray::fromArray([8.0])->log2()->toArray());    // [3.0]
var_dump(NDArray::fromArray([1000.0])->log10()->toArray());// [3.0]

// log on int promotes to f64
$li = NDArray::fromArray([1, 10]);
var_dump($li->log10()->dtype());                           // float64

// abs preserves dtype
var_dump(NDArray::fromArray([-3, 5, -7])->abs()->toArray());     // [3, 5, 7]
var_dump(NDArray::fromArray([-3, 5, -7])->abs()->dtype());       // int64
var_dump(NDArray::fromArray([-1.5, 2.5])->abs()->toArray());     // [1.5, 2.5]

// power — scalar exponent, dtype follows promotion
var_dump(NDArray::fromArray([2, 3, 4])->power(2)->toArray());    // [4, 9, 16]
var_dump(NDArray::fromArray([2, 3, 4])->power(2)->dtype());      // int64
var_dump(NDArray::fromArray([2.0, 3.0])->power(0.5)->toArray()); // [sqrt(2), sqrt(3)]

// clip
$c = NDArray::fromArray([1, 5, 10, 15, 20]);
var_dump($c->clip(5, 15)->toArray());                            // [5, 5, 10, 15, 15]
var_dump($c->clip(null, 12)->toArray());                         // [1, 5, 10, 12, 12]
var_dump($c->clip(5, null)->toArray());                          // [5, 5, 10, 15, 20]

// floor / ceil
$fc = NDArray::fromArray([1.2, -1.2, 1.8, -1.8]);
var_dump($fc->floor()->toArray());                               // [1.0, -2.0, 1.0, -2.0]
var_dump($fc->ceil()->toArray());                                // [2.0, -1.0, 2.0, -1.0]

// floor/ceil on int — no-op, preserves dtype
var_dump(NDArray::fromArray([1, 2, 3])->floor()->dtype());       // int64
var_dump(NDArray::fromArray([1, 2, 3])->floor()->toArray());     // [1, 2, 3]

// round — half-AWAY-FROM-ZERO (PHP_ROUND_HALF_UP), NOT banker's.
// This is a deliberate divergence from NumPy, locked by this test.
$r = NDArray::fromArray([0.5, 1.5, 2.5, -0.5, -1.5]);
var_dump($r->round()->toArray());                                // [1, 2, 3, -1, -2]

// round with decimals
var_dump(NDArray::fromArray([1.2345, 2.3456])->round(2)->toArray()); // [1.23, 2.35]
?>
--EXPECT--
array(2) {
  [0]=>
  array(2) {
    [0]=>
    float(1)
    [1]=>
    float(2)
  }
  [1]=>
  array(2) {
    [0]=>
    float(3)
    [1]=>
    float(4)
  }
}
string(7) "float64"
string(7) "float32"
string(7) "float64"
exp0-ok
exp1-ok
log0-ok
log1-ok
array(1) {
  [0]=>
  float(3)
}
array(1) {
  [0]=>
  float(3)
}
string(7) "float64"
array(3) {
  [0]=>
  int(3)
  [1]=>
  int(5)
  [2]=>
  int(7)
}
string(5) "int64"
array(2) {
  [0]=>
  float(1.5)
  [1]=>
  float(2.5)
}
array(3) {
  [0]=>
  int(4)
  [1]=>
  int(9)
  [2]=>
  int(16)
}
string(5) "int64"
array(2) {
  [0]=>
  float(1.4142135623730951)
  [1]=>
  float(1.7320508075688772)
}
array(5) {
  [0]=>
  int(5)
  [1]=>
  int(5)
  [2]=>
  int(10)
  [3]=>
  int(15)
  [4]=>
  int(15)
}
array(5) {
  [0]=>
  int(1)
  [1]=>
  int(5)
  [2]=>
  int(10)
  [3]=>
  int(12)
  [4]=>
  int(12)
}
array(5) {
  [0]=>
  int(5)
  [1]=>
  int(5)
  [2]=>
  int(10)
  [3]=>
  int(15)
  [4]=>
  int(20)
}
array(4) {
  [0]=>
  float(1)
  [1]=>
  float(-2)
  [2]=>
  float(1)
  [3]=>
  float(-2)
}
array(4) {
  [0]=>
  float(2)
  [1]=>
  float(-1)
  [2]=>
  float(2)
  [3]=>
  float(-1)
}
string(5) "int64"
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
array(5) {
  [0]=>
  float(1)
  [1]=>
  float(2)
  [2]=>
  float(3)
  [3]=>
  float(-1)
  [4]=>
  float(-2)
}
array(2) {
  [0]=>
  float(1.23)
  [1]=>
  float(2.35)
}
