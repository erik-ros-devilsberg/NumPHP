--TEST--
NDArray::full — fills with given value
--FILE--
<?php
var_dump(NDArray::full([3], 7.5)->toArray());
var_dump(NDArray::full([2, 2], -1, 'int64')->toArray());
var_dump(NDArray::full([0], 99)->toArray());          // empty array
?>
--EXPECT--
array(3) {
  [0]=>
  float(7.5)
  [1]=>
  float(7.5)
  [2]=>
  float(7.5)
}
array(2) {
  [0]=>
  array(2) {
    [0]=>
    int(-1)
    [1]=>
    int(-1)
  }
  [1]=>
  array(2) {
    [0]=>
    int(-1)
    [1]=>
    int(-1)
  }
}
array(0) {
}
