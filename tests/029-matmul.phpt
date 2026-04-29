--TEST--
NDArray::matmul — 2-D × 2-D via cblas_*gemm
--FILE--
<?php
$A = NDArray::fromArray([[1, 2], [3, 4]]);
$B = NDArray::fromArray([[5, 6], [7, 8]]);
$C = NDArray::matmul($A, $B);
var_dump($C->dtype());
var_dump($C->toArray());                  // [[19, 22], [43, 50]]

// non-square
$X = NDArray::fromArray([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]);    // [2, 3]
$Y = NDArray::fromArray([[1.0, 0.0], [0.0, 1.0], [1.0, 1.0]]);  // [3, 2]
var_dump(NDArray::matmul($X, $Y)->toArray());

// f32 path
$Af = NDArray::ones([2, 2], 'float32');
$Bf = NDArray::ones([2, 2], 'float32');
$Cf = NDArray::matmul($Af, $Bf);
var_dump($Cf->dtype());
var_dump($Cf->toArray());

// shape mismatch
try {
    NDArray::matmul(NDArray::ones([2, 3]), NDArray::ones([4, 2]));
} catch (ShapeException $e) { echo "mismatch: ", $e->getMessage(), "\n"; }

// 1D rejected (use dot)
try {
    NDArray::matmul(NDArray::ones([3]), NDArray::ones([3]));
} catch (ShapeException $e) { echo "not-2d: ", $e->getMessage(), "\n"; }
?>
--EXPECT--
string(7) "float64"
array(2) {
  [0]=>
  array(2) {
    [0]=>
    float(19)
    [1]=>
    float(22)
  }
  [1]=>
  array(2) {
    [0]=>
    float(43)
    [1]=>
    float(50)
  }
}
array(2) {
  [0]=>
  array(2) {
    [0]=>
    float(4)
    [1]=>
    float(5)
  }
  [1]=>
  array(2) {
    [0]=>
    float(10)
    [1]=>
    float(11)
  }
}
string(7) "float32"
array(2) {
  [0]=>
  array(2) {
    [0]=>
    float(2)
    [1]=>
    float(2)
  }
  [1]=>
  array(2) {
    [0]=>
    float(2)
    [1]=>
    float(2)
  }
}
mismatch: matmul: shape mismatch (A cols 3 != B rows 4)
not-2d: matmul: both inputs must be 2-D in v1 (batched matmul deferred)
