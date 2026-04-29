--TEST--
NDArray::stack — adds a new axis
--FILE--
<?php
$a = NDArray::fromArray([1, 2, 3]);
$b = NDArray::fromArray([4, 5, 6]);
$c = NDArray::fromArray([7, 8, 9]);

var_dump(NDArray::stack([$a, $b, $c])->toArray());        // axis 0, shape [3, 3]
var_dump(NDArray::stack([$a, $b, $c])->shape());
var_dump(NDArray::stack([$a, $b, $c], 1)->toArray());     // axis 1, shape [3, 3]
var_dump(NDArray::stack([$a, $b, $c], 1)->shape());

// 2D stack
$m1 = NDArray::fromArray([[1, 2], [3, 4]]);
$m2 = NDArray::fromArray([[5, 6], [7, 8]]);
var_dump(NDArray::stack([$m1, $m2])->shape());            // [2, 2, 2]

// shape mismatch
try {
    NDArray::stack([$a, NDArray::fromArray([1, 2])]);
} catch (ShapeException $e) {
    echo "shape-mismatch: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
array(3) {
  [0]=>
  array(3) {
    [0]=>
    int(1)
    [1]=>
    int(2)
    [2]=>
    int(3)
  }
  [1]=>
  array(3) {
    [0]=>
    int(4)
    [1]=>
    int(5)
    [2]=>
    int(6)
  }
  [2]=>
  array(3) {
    [0]=>
    int(7)
    [1]=>
    int(8)
    [2]=>
    int(9)
  }
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
    int(1)
    [1]=>
    int(4)
    [2]=>
    int(7)
  }
  [1]=>
  array(3) {
    [0]=>
    int(2)
    [1]=>
    int(5)
    [2]=>
    int(8)
  }
  [2]=>
  array(3) {
    [0]=>
    int(3)
    [1]=>
    int(6)
    [2]=>
    int(9)
  }
}
array(2) {
  [0]=>
  int(3)
  [1]=>
  int(3)
}
array(3) {
  [0]=>
  int(2)
  [1]=>
  int(2)
  [2]=>
  int(2)
}
shape-mismatch: stack: shape mismatch
