--TEST--
Operator overloading: $a + $b, scalar variants, compound assignment
--FILE--
<?php
$a = NDArray::fromArray([1, 2, 3]);
$b = NDArray::fromArray([10, 20, 30]);

var_dump(($a + $b)->toArray());
var_dump(($b - $a)->toArray());
var_dump(($a * $b)->toArray());

// scalar on either side
var_dump(($a + 5)->toArray());
var_dump((10 - $a)->toArray());
var_dump(($a * 2.5)->toArray());

// compound assignment composes via $a = $a + $b
$c = NDArray::fromArray([1, 1, 1]);
$c += NDArray::fromArray([10, 20, 30]);
var_dump($c->toArray());
?>
--EXPECT--
array(3) {
  [0]=>
  int(11)
  [1]=>
  int(22)
  [2]=>
  int(33)
}
array(3) {
  [0]=>
  int(9)
  [1]=>
  int(18)
  [2]=>
  int(27)
}
array(3) {
  [0]=>
  int(10)
  [1]=>
  int(40)
  [2]=>
  int(90)
}
array(3) {
  [0]=>
  int(6)
  [1]=>
  int(7)
  [2]=>
  int(8)
}
array(3) {
  [0]=>
  int(9)
  [1]=>
  int(8)
  [2]=>
  int(7)
}
array(3) {
  [0]=>
  float(2.5)
  [1]=>
  float(5)
  [2]=>
  float(7.5)
}
array(3) {
  [0]=>
  int(11)
  [1]=>
  int(21)
  [2]=>
  int(31)
}
