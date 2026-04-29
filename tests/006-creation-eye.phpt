--TEST--
NDArray::eye — square, non-square, off-diagonal
--FILE--
<?php
var_dump(NDArray::eye(3)->toArray());
var_dump(NDArray::eye(2, 4)->toArray());
var_dump(NDArray::eye(3, k: 1)->toArray());
var_dump(NDArray::eye(3, k: -1)->toArray());
var_dump(NDArray::eye(2, dtype: 'int32')->toArray());
?>
--EXPECT--
array(3) {
  [0]=>
  array(3) {
    [0]=>
    float(1)
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
    float(1)
    [2]=>
    float(0)
  }
  [2]=>
  array(3) {
    [0]=>
    float(0)
    [1]=>
    float(0)
    [2]=>
    float(1)
  }
}
array(2) {
  [0]=>
  array(4) {
    [0]=>
    float(1)
    [1]=>
    float(0)
    [2]=>
    float(0)
    [3]=>
    float(0)
  }
  [1]=>
  array(4) {
    [0]=>
    float(0)
    [1]=>
    float(1)
    [2]=>
    float(0)
    [3]=>
    float(0)
  }
}
array(3) {
  [0]=>
  array(3) {
    [0]=>
    float(0)
    [1]=>
    float(1)
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
    float(1)
  }
  [2]=>
  array(3) {
    [0]=>
    float(0)
    [1]=>
    float(0)
    [2]=>
    float(0)
  }
}
array(3) {
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
    float(1)
    [1]=>
    float(0)
    [2]=>
    float(0)
  }
  [2]=>
  array(3) {
    [0]=>
    float(0)
    [1]=>
    float(1)
    [2]=>
    float(0)
  }
}
array(2) {
  [0]=>
  array(2) {
    [0]=>
    int(1)
    [1]=>
    int(0)
  }
  [1]=>
  array(2) {
    [0]=>
    int(0)
    [1]=>
    int(1)
  }
}
