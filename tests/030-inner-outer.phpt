--TEST--
NDArray::inner (1-D only) and ::outer (1-D × 1-D → 2-D)
--FILE--
<?php
$a = NDArray::fromArray([1.0, 2.0, 3.0]);
$b = NDArray::fromArray([4.0, 5.0, 6.0]);
var_dump(NDArray::inner($a, $b));         // 32

$o = NDArray::outer($a, $b);
var_dump($o->shape());
var_dump($o->toArray());                  // [[4,5,6],[8,10,12],[12,15,18]]

// outer with different lengths
$x = NDArray::fromArray([1.0, 2.0]);
$y = NDArray::fromArray([10.0, 20.0, 30.0, 40.0]);
var_dump(NDArray::outer($x, $y)->shape()); // [2, 4]

// inner higher-D rejected
try {
    NDArray::inner(NDArray::ones([3, 3]), NDArray::ones([3, 3]));
} catch (ShapeException $e) { echo "inner-nd: ", $e->getMessage(), "\n"; }
?>
--EXPECT--
float(32)
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
    float(4)
    [1]=>
    float(5)
    [2]=>
    float(6)
  }
  [1]=>
  array(3) {
    [0]=>
    float(8)
    [1]=>
    float(10)
    [2]=>
    float(12)
  }
  [2]=>
  array(3) {
    [0]=>
    float(12)
    [1]=>
    float(15)
    [2]=>
    float(18)
  }
}
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(4)
}
inner-nd: inner: only 1-D inputs supported in v1 (use matmul for higher-D)
