--TEST--
NDArray::squeeze and ::expandDims
--FILE--
<?php
$a = NDArray::zeros([1, 3, 1, 4]);
var_dump($a->squeeze()->shape());                  // [3, 4]
var_dump($a->squeeze(0)->shape());                 // [3, 1, 4]
var_dump($a->squeeze(2)->shape());                 // [1, 3, 4]

try {
    $a->squeeze(-1);                                // axis 3, shape[3]=4 ≠ 1 → throws
} catch (ShapeException $e) {
    echo "non-1: ", $e->getMessage(), "\n";
}

$b = NDArray::fromArray([1, 2, 3]);
var_dump($b->expandDims(0)->shape());              // [1, 3]
var_dump($b->expandDims(1)->shape());              // [3, 1]
var_dump($b->expandDims(-1)->shape());             // [3, 1]
var_dump($b->expandDims(0)->squeeze(0)->toArray());

try {
    $a->squeeze(1);                                 // axis 1, shape[1]=3 ≠ 1
} catch (ShapeException $e) {
    echo "non-1-mid: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
array(2) {
  [0]=>
  int(3)
  [1]=>
  int(4)
}
array(3) {
  [0]=>
  int(3)
  [1]=>
  int(1)
  [2]=>
  int(4)
}
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(3)
  [2]=>
  int(4)
}
non-1: squeeze: cannot squeeze axis with size != 1
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(3)
}
array(2) {
  [0]=>
  int(3)
  [1]=>
  int(1)
}
array(2) {
  [0]=>
  int(3)
  [1]=>
  int(1)
}
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
non-1-mid: squeeze: cannot squeeze axis with size != 1
