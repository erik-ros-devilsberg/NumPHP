--TEST--
NDArray::flatten — always returns contiguous 1-D copy
--FILE--
<?php
$a = NDArray::fromArray([[1, 2, 3], [4, 5, 6]]);
var_dump($a->flatten()->toArray());
var_dump($a->flatten()->shape());

// flatten of transpose preserves data ORDER of transposed traversal
$at = $a->transpose();
var_dump($at->toArray());
var_dump($at->flatten()->toArray());      // 1, 4, 2, 5, 3, 6

// 0-D scalar → 1-D length 1
$sc = NDArray::full([], 42, 'int64');
var_dump($sc->flatten()->shape());
var_dump($sc->flatten()->toArray());

// empty
$e = NDArray::zeros([0]);
var_dump($e->flatten()->shape());
?>
--EXPECT--
array(6) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
  [3]=>
  int(4)
  [4]=>
  int(5)
  [5]=>
  int(6)
}
array(1) {
  [0]=>
  int(6)
}
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
array(6) {
  [0]=>
  int(1)
  [1]=>
  int(4)
  [2]=>
  int(2)
  [3]=>
  int(5)
  [4]=>
  int(3)
  [5]=>
  int(6)
}
array(1) {
  [0]=>
  int(1)
}
array(1) {
  [0]=>
  int(42)
}
array(1) {
  [0]=>
  int(0)
}
