--TEST--
NDArray metadata accessors: shape / dtype / size / ndim
--FILE--
<?php
$a = NDArray::zeros([3, 4, 5], 'int64');
var_dump($a->shape());
var_dump($a->dtype());
var_dump($a->size());
var_dump($a->ndim());

$b = NDArray::zeros([7]);
var_dump($b->shape(), $b->ndim(), $b->size());

$c = NDArray::zeros([], 'float32');
var_dump($c->shape(), $c->ndim(), $c->size());      // 0-D scalar
?>
--EXPECT--
array(3) {
  [0]=>
  int(3)
  [1]=>
  int(4)
  [2]=>
  int(5)
}
string(5) "int64"
int(60)
int(3)
array(1) {
  [0]=>
  int(7)
}
int(1)
int(7)
array(0) {
}
int(0)
int(1)
