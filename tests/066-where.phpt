--TEST--
NDArray::where — element select with broadcasting and dtype promotion
--FILE--
<?php
// All-array case
$cond = NDArray::fromArray([true, false, true, false], 'bool');
$x    = NDArray::fromArray([10, 20, 30, 40]);
$y    = NDArray::fromArray([-1, -2, -3, -4]);
var_dump(NDArray::where($cond, $x, $y)->toArray());     // [10, -2, 30, -4]

// Scalar x and y
var_dump(NDArray::where($cond, 1.0, 0.0)->toArray());   // [1.0, 0.0, 1.0, 0.0]
var_dump(NDArray::where($cond, 1.0, 0.0)->dtype());     // float64

// Mixed scalar + array  ("clip negatives to zero" / relu)
$v = NDArray::fromArray([-2.0, -1.0, 0.0, 1.0, 2.0]);
$pos = NDArray::gt($v, 0);
var_dump(NDArray::where($pos, $v, 0.0)->toArray());     // [0, 0, 0, 1, 2]

// Broadcasting all three operands
$cb = NDArray::zeros([3, 1], 'bool');                   // 3x1 false
$xb = NDArray::ones([1, 3], 'float64');                 // 1x3 ones
$yb = NDArray::full([1, 3], 7.0, 'float64');            // 1x3 sevens
$wb = NDArray::where($cb, $xb, $yb);
var_dump($wb->shape());                                  // [3, 3]
var_dump($wb->toArray());                                // all 7.0

// Output dtype follows promotion of x and y; cond's dtype is irrelevant
$xi32 = NDArray::fromArray([1, 2, 3, 4], 'int32');
$xf64 = NDArray::fromArray([0.5, 1.5, 2.5, 3.5], 'float64');
var_dump(NDArray::where($cond, $xi32, $xf64)->dtype()); // float64

// cond not bool → DTypeException
try {
    NDArray::where(
        NDArray::fromArray([1, 0, 1, 0], 'int32'),
        $x, $y);
    echo "FAIL\n";
} catch (DTypeException $e) {
    echo "cond-not-bool-rejected\n";
}

// Shape mismatch among cond/x/y
try {
    NDArray::where(
        NDArray::fromArray([true, false, true], 'bool'),
        NDArray::fromArray([1, 2]),
        NDArray::fromArray([3, 4]));
    echo "FAIL\n";
} catch (ShapeException $e) {
    echo "where-shape-mismatch-ok\n";
}
?>
--EXPECT--
array(4) {
  [0]=>
  int(10)
  [1]=>
  int(-2)
  [2]=>
  int(30)
  [3]=>
  int(-4)
}
array(4) {
  [0]=>
  float(1)
  [1]=>
  float(0)
  [2]=>
  float(1)
  [3]=>
  float(0)
}
string(7) "float64"
array(5) {
  [0]=>
  float(0)
  [1]=>
  float(0)
  [2]=>
  float(0)
  [3]=>
  float(1)
  [4]=>
  float(2)
}
array(2) {
  [0]=>
  int(3)
  [1]=>
  int(3)
}
array(3) {
  [0]=>
  array(3) {
    [0]=>
    float(7)
    [1]=>
    float(7)
    [2]=>
    float(7)
  }
  [1]=>
  array(3) {
    [0]=>
    float(7)
    [1]=>
    float(7)
    [2]=>
    float(7)
  }
  [2]=>
  array(3) {
    [0]=>
    float(7)
    [1]=>
    float(7)
    [2]=>
    float(7)
  }
}
string(7) "float64"
cond-not-bool-rejected
where-shape-mismatch-ok
