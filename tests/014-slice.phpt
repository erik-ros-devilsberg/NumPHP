--TEST--
NDArray::slice — positive and negative bounds, step
--FILE--
<?php
$a = NDArray::arange(0, 10);
var_dump($a->slice(2, 7)->toArray());
var_dump($a->slice(0, 10, 2)->toArray());
var_dump($a->slice(-3, 10)->toArray());
var_dump($a->slice(0, -2)->toArray());
var_dump($a->slice(5, 5)->toArray());          // empty
var_dump($a->slice(20, 30)->toArray());        // entirely past end → empty

$b = NDArray::fromArray([[1, 2, 3], [4, 5, 6], [7, 8, 9], [10, 11, 12]]);
var_dump($b->slice(1, 3)->toArray());
var_dump($b->slice(0, 4, 2)->toArray());

try {
    $a->slice(0, 5, -1);
} catch (NDArrayException $e) {
    echo "neg-step: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
array(5) {
  [0]=>
  int(2)
  [1]=>
  int(3)
  [2]=>
  int(4)
  [3]=>
  int(5)
  [4]=>
  int(6)
}
array(5) {
  [0]=>
  int(0)
  [1]=>
  int(2)
  [2]=>
  int(4)
  [3]=>
  int(6)
  [4]=>
  int(8)
}
array(3) {
  [0]=>
  int(7)
  [1]=>
  int(8)
  [2]=>
  int(9)
}
array(8) {
  [0]=>
  int(0)
  [1]=>
  int(1)
  [2]=>
  int(2)
  [3]=>
  int(3)
  [4]=>
  int(4)
  [5]=>
  int(5)
  [6]=>
  int(6)
  [7]=>
  int(7)
}
array(0) {
}
array(0) {
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    int(4)
    [1]=>
    int(5)
    [2]=>
    int(6)
  }
  [1]=>
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
    int(7)
    [1]=>
    int(8)
    [2]=>
    int(9)
  }
}
neg-step: slice: step must be positive (negative step deferred)
