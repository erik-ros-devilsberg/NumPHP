--TEST--
ArrayAccess offsetGet — 1D scalar, nD view, negative index, OOB
--FILE--
<?php
$a = NDArray::fromArray([10, 20, 30, 40]);
var_dump($a[0]);
var_dump($a[3]);
var_dump($a[-1]);
var_dump($a[-4]);

$b = NDArray::fromArray([[1, 2, 3], [4, 5, 6]]);
$row = $b[1];
var_dump($row->shape());
var_dump($row->toArray());
var_dump($b[0][2]);          // chained indexing → scalar
var_dump($b[-1][-1]);

$c = NDArray::fromArray([[[1, 2], [3, 4]], [[5, 6], [7, 8]]]);
var_dump($c[0]->shape());
var_dump($c[0][1]->toArray());
var_dump($c[1][0][1]);

try { $a[4]; } catch (IndexException $e) { echo "oob-pos: ", $e->getMessage(), "\n"; }
try { $a[-5]; } catch (IndexException $e) { echo "oob-neg: ", $e->getMessage(), "\n"; }
?>
--EXPECT--
int(10)
int(40)
int(40)
int(10)
array(1) {
  [0]=>
  int(3)
}
array(3) {
  [0]=>
  int(4)
  [1]=>
  int(5)
  [2]=>
  int(6)
}
int(3)
int(6)
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(2)
}
array(2) {
  [0]=>
  int(3)
  [1]=>
  int(4)
}
int(6)
oob-pos: Index 4 out of bounds for axis 0 with size 4
oob-neg: Index -5 out of bounds for axis 0 with size 4
