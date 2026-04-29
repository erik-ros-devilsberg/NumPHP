--TEST--
NDArray::dot — 1-D inner product via cblas_*dot
--FILE--
<?php
$a = NDArray::fromArray([1.0, 2.0, 3.0]);
$b = NDArray::fromArray([4.0, 5.0, 6.0]);
var_dump(NDArray::dot($a, $b));         // 1*4 + 2*5 + 3*6 = 32

// int operands → promoted to f64, returns float
$ai = NDArray::fromArray([1, 2, 3]);
$bi = NDArray::fromArray([4, 5, 6]);
var_dump(NDArray::dot($ai, $bi));

// f32 path
$af = NDArray::ones([4], 'float32');
$bf = NDArray::full([4], 0.5, 'float32');
var_dump(NDArray::dot($af, $bf));        // 4 * 1 * 0.5 = 2.0

// shape mismatch
try {
    NDArray::dot(NDArray::ones([3]), NDArray::ones([4]));
} catch (ShapeException $e) { echo "mismatch: ", $e->getMessage(), "\n"; }

// rank > 1
try {
    NDArray::dot(NDArray::ones([3, 3]), NDArray::ones([3, 3]));
} catch (ShapeException $e) { echo "not-1d: ", $e->getMessage(), "\n"; }
?>
--EXPECT--
float(32)
float(32)
float(2)
mismatch: dot: shape mismatch (3 vs 4)
not-1d: dot: both inputs must be 1-D
