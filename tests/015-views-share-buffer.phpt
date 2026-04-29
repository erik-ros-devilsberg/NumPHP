--TEST--
Views share the parent buffer; clone deep-copies
--FILE--
<?php
$parent = NDArray::zeros([3, 3], 'int64');
$row1 = $parent[1];

// Mutating through the view affects the parent
$row1[0] = 100;
$row1[1] = 200;
$row1[2] = 300;
var_dump($parent->toArray());

// Slicing produces a view — also shares buffer
$top = $parent->slice(0, 2);
$top[0] = 1;                  // scalar broadcast row 0
var_dump($parent->toArray());

// Clone the view → independent deep copy; mutations no longer propagate
$independent = clone $row1;
$independent[0] = 999;
var_dump($row1->toArray());           // unchanged
var_dump($independent->toArray());

// Owner can be unset while view holds buffer → no use-after-free
$big = NDArray::arange(0, 5);
$tail = $big->slice(2, 5);
unset($big);
var_dump($tail->toArray());
?>
--EXPECT--
array(3) {
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
    int(100)
    [1]=>
    int(200)
    [2]=>
    int(300)
  }
  [2]=>
  array(3) {
    [0]=>
    int(0)
    [1]=>
    int(0)
    [2]=>
    int(0)
  }
}
array(3) {
  [0]=>
  array(3) {
    [0]=>
    int(1)
    [1]=>
    int(1)
    [2]=>
    int(1)
  }
  [1]=>
  array(3) {
    [0]=>
    int(100)
    [1]=>
    int(200)
    [2]=>
    int(300)
  }
  [2]=>
  array(3) {
    [0]=>
    int(0)
    [1]=>
    int(0)
    [2]=>
    int(0)
  }
}
array(3) {
  [0]=>
  int(100)
  [1]=>
  int(200)
  [2]=>
  int(300)
}
array(3) {
  [0]=>
  int(999)
  [1]=>
  int(200)
  [2]=>
  int(300)
}
array(3) {
  [0]=>
  int(2)
  [1]=>
  int(3)
  [2]=>
  int(4)
}
