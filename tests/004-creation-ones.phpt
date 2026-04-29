--TEST--
NDArray::ones — all dtypes
--FILE--
<?php
$a = NDArray::ones([3]);
var_dump($a->dtype());
var_dump($a->toArray());

$b = NDArray::ones([2, 2], 'int32');
var_dump($b->dtype());
var_dump($b->toArray());
?>
--EXPECT--
string(7) "float64"
array(3) {
  [0]=>
  float(1)
  [1]=>
  float(1)
  [2]=>
  float(1)
}
string(5) "int32"
array(2) {
  [0]=>
  array(2) {
    [0]=>
    int(1)
    [1]=>
    int(1)
  }
  [1]=>
  array(2) {
    [0]=>
    int(1)
    [1]=>
    int(1)
  }
}
