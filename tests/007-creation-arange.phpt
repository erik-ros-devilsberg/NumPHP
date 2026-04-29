--TEST--
NDArray::arange — int default, float default, explicit dtype, step variants
--FILE--
<?php
$a = NDArray::arange(0, 5);                   // int args → int64
var_dump($a->dtype(), $a->toArray());

$b = NDArray::arange(0.0, 5.0);               // float arg → float64
var_dump($b->dtype(), $b->toArray());

$c = NDArray::arange(0, 10, 2);
var_dump($c->dtype(), $c->toArray());

$d = NDArray::arange(5, 0, -1);
var_dump($d->dtype(), $d->toArray());

$e = NDArray::arange(0, 5, 1, 'float32');     // explicit dtype
var_dump($e->dtype(), $e->toArray());

$f = NDArray::arange(0, 0);                   // empty
var_dump($f->size(), $f->toArray());
?>
--EXPECT--
string(5) "int64"
array(5) {
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
}
string(7) "float64"
array(5) {
  [0]=>
  float(0)
  [1]=>
  float(1)
  [2]=>
  float(2)
  [3]=>
  float(3)
  [4]=>
  float(4)
}
string(5) "int64"
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
string(5) "int64"
array(5) {
  [0]=>
  int(5)
  [1]=>
  int(4)
  [2]=>
  int(3)
  [3]=>
  int(2)
  [4]=>
  int(1)
}
string(7) "float32"
array(5) {
  [0]=>
  float(0)
  [1]=>
  float(1)
  [2]=>
  float(2)
  [3]=>
  float(3)
  [4]=>
  float(4)
}
int(0)
array(0) {
}
