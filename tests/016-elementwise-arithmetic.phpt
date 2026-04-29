--TEST--
Element-wise add / subtract / multiply / divide on matching shapes
--FILE--
<?php
$a = NDArray::fromArray([[1, 2, 3], [4, 5, 6]]);
$b = NDArray::fromArray([[10, 20, 30], [40, 50, 60]]);

var_dump(NDArray::add($a, $b)->toArray());
var_dump(NDArray::subtract($b, $a)->toArray());
var_dump(NDArray::multiply($a, $b)->toArray());

$x = NDArray::fromArray([10, 20, 30, 40]);
$y = NDArray::fromArray([2, 4, 5, 8]);
var_dump(NDArray::divide($x, $y)->toArray());

// 1D float
$f = NDArray::fromArray([1.0, 2.0, 4.0]);
$g = NDArray::fromArray([0.5, 0.5, 0.5]);
var_dump(NDArray::multiply($f, $g)->toArray());
?>
--EXPECT--
array(2) {
  [0]=>
  array(3) {
    [0]=>
    int(11)
    [1]=>
    int(22)
    [2]=>
    int(33)
  }
  [1]=>
  array(3) {
    [0]=>
    int(44)
    [1]=>
    int(55)
    [2]=>
    int(66)
  }
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    int(9)
    [1]=>
    int(18)
    [2]=>
    int(27)
  }
  [1]=>
  array(3) {
    [0]=>
    int(36)
    [1]=>
    int(45)
    [2]=>
    int(54)
  }
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    int(10)
    [1]=>
    int(40)
    [2]=>
    int(90)
  }
  [1]=>
  array(3) {
    [0]=>
    int(160)
    [1]=>
    int(250)
    [2]=>
    int(360)
  }
}
array(4) {
  [0]=>
  int(5)
  [1]=>
  int(5)
  [2]=>
  int(6)
  [3]=>
  int(5)
}
array(3) {
  [0]=>
  float(0.5)
  [1]=>
  float(1)
  [2]=>
  float(2)
}
