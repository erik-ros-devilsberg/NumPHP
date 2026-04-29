--TEST--
NDArray::zeros — all dtypes, shape preserved, all elements zero
--FILE--
<?php
foreach (['float64', 'float32', 'int64', 'int32'] as $dt) {
    $a = NDArray::zeros([2, 3], $dt);
    var_dump($a->shape());
    var_dump($a->dtype());
    var_dump($a->size());
    var_dump($a->ndim());
    var_dump($a->toArray());
}
$d = NDArray::zeros([4]);            // default dtype
var_dump($d->dtype());
var_dump($d->toArray());
?>
--EXPECT--
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(3)
}
string(7) "float64"
int(6)
int(2)
array(2) {
  [0]=>
  array(3) {
    [0]=>
    float(0)
    [1]=>
    float(0)
    [2]=>
    float(0)
  }
  [1]=>
  array(3) {
    [0]=>
    float(0)
    [1]=>
    float(0)
    [2]=>
    float(0)
  }
}
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(3)
}
string(7) "float32"
int(6)
int(2)
array(2) {
  [0]=>
  array(3) {
    [0]=>
    float(0)
    [1]=>
    float(0)
    [2]=>
    float(0)
  }
  [1]=>
  array(3) {
    [0]=>
    float(0)
    [1]=>
    float(0)
    [2]=>
    float(0)
  }
}
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(3)
}
string(5) "int64"
int(6)
int(2)
array(2) {
  [0]=>
  array(3) {
    [0]=>
    int(0)
    [1]=>
    int(0)
    [2]=>
    int(0)
  }
  [1]=>
  array(3) {
    [0]=>
    int(0)
    [1]=>
    int(0)
    [2]=>
    int(0)
  }
}
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(3)
}
string(5) "int32"
int(6)
int(2)
array(2) {
  [0]=>
  array(3) {
    [0]=>
    int(0)
    [1]=>
    int(0)
    [2]=>
    int(0)
  }
  [1]=>
  array(3) {
    [0]=>
    int(0)
    [1]=>
    int(0)
    [2]=>
    int(0)
  }
}
string(7) "float64"
array(4) {
  [0]=>
  float(0)
  [1]=>
  float(0)
  [2]=>
  float(0)
  [3]=>
  float(0)
}
