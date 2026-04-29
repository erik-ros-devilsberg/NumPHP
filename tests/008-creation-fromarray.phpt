--TEST--
NDArray::fromArray — shape inference, dtype inference, ragged rejection
--FILE--
<?php
$a = NDArray::fromArray([1, 2, 3]);
var_dump($a->dtype(), $a->shape(), $a->toArray());

$b = NDArray::fromArray([[1, 2], [3, 4]]);
var_dump($b->dtype(), $b->shape(), $b->toArray());

$c = NDArray::fromArray([1, 2.5, 3]);                // mixed → float64
var_dump($c->dtype(), $c->toArray());

$d = NDArray::fromArray([[1, 2, 3], [4, 5, 6]], 'int32');
var_dump($d->dtype(), $d->toArray());

$e = NDArray::fromArray([[[1, 2], [3, 4]], [[5, 6], [7, 8]]]);
var_dump($e->shape());

try {
    NDArray::fromArray([[1, 2], [3]]);
} catch (ShapeException $ex) {
    echo "ragged-rows: ", $ex->getMessage(), "\n";
}

try {
    NDArray::fromArray([[1, 2], 3]);
} catch (ShapeException $ex) {
    echo "ragged-mixed: ", $ex->getMessage(), "\n";
}
?>
--EXPECT--
string(5) "int64"
array(1) {
  [0]=>
  int(3)
}
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
string(5) "int64"
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(2)
}
array(2) {
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
}
string(7) "float64"
array(3) {
  [0]=>
  float(1)
  [1]=>
  float(2.5)
  [2]=>
  float(3)
}
string(5) "int32"
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
array(3) {
  [0]=>
  int(2)
  [1]=>
  int(2)
  [2]=>
  int(2)
}
ragged-rows: Ragged array: inconsistent dimension lengths
ragged-mixed: Ragged array: scalar at non-leaf depth
