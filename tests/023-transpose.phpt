--TEST--
NDArray::transpose — default reverse, explicit permutation
--FILE--
<?php
$a = NDArray::fromArray([[1, 2, 3], [4, 5, 6]]);
var_dump($a->transpose()->toArray());

$cube = NDArray::arange(0, 24)->reshape([2, 3, 4]);
var_dump($cube->transpose()->shape());
var_dump($cube->transpose([1, 0, 2])->shape());
var_dump($cube->transpose([0, 2, 1])->shape());

// transpose ∘ transpose === identity for 2D
$id = $a->transpose()->transpose();
var_dump($id->toArray());

// invalid: duplicate axis
try { $a->transpose([0, 0]); } catch (ShapeException $e) { echo "dup-axis: ", $e->getMessage(), "\n"; }

// invalid: wrong length
try { $cube->transpose([0, 1]); } catch (ShapeException $e) { echo "wrong-len: ", $e->getMessage(), "\n"; }
?>
--EXPECT--
array(3) {
  [0]=>
  array(2) {
    [0]=>
    int(1)
    [1]=>
    int(4)
  }
  [1]=>
  array(2) {
    [0]=>
    int(2)
    [1]=>
    int(5)
  }
  [2]=>
  array(2) {
    [0]=>
    int(3)
    [1]=>
    int(6)
  }
}
array(3) {
  [0]=>
  int(4)
  [1]=>
  int(3)
  [2]=>
  int(2)
}
array(3) {
  [0]=>
  int(3)
  [1]=>
  int(2)
  [2]=>
  int(4)
}
array(3) {
  [0]=>
  int(2)
  [1]=>
  int(4)
  [2]=>
  int(3)
}
array(2) {
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
}
dup-axis: transpose: invalid or duplicate axis
wrong-len: transpose: axes must have ndim entries
