--TEST--
Stats edge cases — 0-D, empty axis, axis bounds
--FILE--
<?php
// 0-D reduction (scalar input)
$z = NDArray::full([], 7.5, 'float64');                 // shape []
var_dump($z->ndim());                                    // 0
var_dump($z->sum());                                     // float(7.5)
var_dump($z->mean());                                    // float(7.5)
var_dump($z->min());                                     // float(7.5)
var_dump($z->argmin());                                  // 0

// Reduction on shape with size-1 axis
$one = NDArray::fromArray([[5]]);                        // shape [1, 1]
var_dump($one->sum(0)->toArray());                       // [5]
var_dump($one->sum(1)->toArray());                       // [5]
var_dump($one->mean());                                  // float(5)

// Out-of-range axis on every reduction
$a = NDArray::fromArray([[1, 2], [3, 4]]);
foreach (['sum', 'mean', 'min', 'max', 'var', 'std', 'argmin', 'argmax'] as $m) {
    try { $a->$m(5); echo "FAIL $m\n"; }
    catch (ShapeException $e) { echo "$m: ", $e->getMessage(), "\n"; }
}

// Empty array (shape [0]) — sum of empty is 0; mean is NaN; argmin throws.
// zeros() defaults to float64, so sum returns float(0), not int(0).
$empty = NDArray::zeros([0]);
var_dump($empty->size());                                // 0
var_dump($empty->sum());                                 // float(0) — additive identity, dtype-preserving
echo is_nan($empty->mean()) ? "empty-mean-nan\n" : "FAIL\n";  // mean of empty = 0/0 = NaN

try { $empty->argmin(); echo "FAIL\n"; }
catch (NDArrayException $e) { echo "empty-argmin: ", $e->getMessage(), "\n"; }

try { $empty->argmax(); echo "FAIL\n"; }
catch (NDArrayException $e) { echo "empty-argmax: ", $e->getMessage(), "\n"; }

// keepdims with axis=null produces all-1 shape
$k = $a->sum(null, true);
var_dump($k->ndim());                                    // 2
var_dump($k->shape());                                   // [1, 1]
var_dump($k->toArray());                                 // [[10]]
?>
--EXPECT--
int(0)
float(7.5)
float(7.5)
float(7.5)
int(0)
array(1) {
  [0]=>
  int(5)
}
array(1) {
  [0]=>
  int(5)
}
float(5)
sum: axis 5 out of range for ndim 2
mean: axis 5 out of range for ndim 2
min: axis 5 out of range for ndim 2
max: axis 5 out of range for ndim 2
var: axis 5 out of range for ndim 2
std: axis 5 out of range for ndim 2
argmin: axis 5 out of range for ndim 2
argmax: axis 5 out of range for ndim 2
int(0)
float(0)
empty-mean-nan
empty-argmin: argmin: empty array
empty-argmax: argmax: empty array
int(2)
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(1)
}
array(1) {
  [0]=>
  array(1) {
    [0]=>
    int(10)
  }
}
