--TEST--
NDArray::concatenate — joins along axis
--FILE--
<?php
$a = NDArray::fromArray([[1, 2], [3, 4]]);
$b = NDArray::fromArray([[5, 6], [7, 8]]);

var_dump(NDArray::concatenate([$a, $b], 0)->toArray());  // [4, 2]
var_dump(NDArray::concatenate([$a, $b], 1)->toArray());  // [2, 4]
var_dump(NDArray::concatenate([$a, $b], -1)->toArray()); // axis -1 = 1

// dtype promotion across inputs
$i = NDArray::fromArray([1, 2], 'int32');
$f = NDArray::fromArray([3.5, 4.5]);
var_dump(NDArray::concatenate([$i, $f])->dtype());

// shape mismatch off concat axis
try {
    $bad = NDArray::fromArray([[5, 6, 9], [7, 8, 10]]);
    NDArray::concatenate([$a, $bad], 0);
} catch (ShapeException $e) {
    echo "off-axis: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
array(4) {
  [0]=>
  array(2) {
    [0]=>
    int(1)
    [1]=>
    int(2)
  }
  [1]=>
  array(2) {
    [0]=>
    int(3)
    [1]=>
    int(4)
  }
  [2]=>
  array(2) {
    [0]=>
    int(5)
    [1]=>
    int(6)
  }
  [3]=>
  array(2) {
    [0]=>
    int(7)
    [1]=>
    int(8)
  }
}
array(2) {
  [0]=>
  array(4) {
    [0]=>
    int(1)
    [1]=>
    int(2)
    [2]=>
    int(5)
    [3]=>
    int(6)
  }
  [1]=>
  array(4) {
    [0]=>
    int(3)
    [1]=>
    int(4)
    [2]=>
    int(7)
    [3]=>
    int(8)
  }
}
array(2) {
  [0]=>
  array(4) {
    [0]=>
    int(1)
    [1]=>
    int(2)
    [2]=>
    int(5)
    [3]=>
    int(6)
  }
  [1]=>
  array(4) {
    [0]=>
    int(3)
    [1]=>
    int(4)
    [2]=>
    int(7)
    [3]=>
    int(8)
  }
}
string(7) "float64"
off-axis: concatenate: shape mismatch off concat axis
