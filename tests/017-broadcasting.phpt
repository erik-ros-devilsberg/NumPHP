--TEST--
Broadcasting: scalar+array, row+matrix, col+matrix, 3D+1D
--FILE--
<?php
// 1D row broadcasts across rows of 2D matrix
$mat = NDArray::fromArray([[1, 2, 3], [4, 5, 6], [7, 8, 9]]);
$row = NDArray::fromArray([10, 20, 30]);
var_dump(NDArray::add($mat, $row)->toArray());

// column broadcast: shape [3,1] + shape [3,3]
$col = NDArray::fromArray([[100], [200], [300]]);
var_dump(NDArray::add($mat, $col)->toArray());

// 3D + 1D
$cube = NDArray::fromArray([[[1, 2], [3, 4]], [[5, 6], [7, 8]]]);
$pair = NDArray::fromArray([10, 20]);
var_dump(NDArray::add($cube, $pair)->toArray());

// scalar wrapped to 0-D
$sc = NDArray::full([], 100, 'int64');
var_dump(NDArray::add($mat, $sc)->toArray());
?>
--EXPECT--
array(3) {
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
    int(14)
    [1]=>
    int(25)
    [2]=>
    int(36)
  }
  [2]=>
  array(3) {
    [0]=>
    int(17)
    [1]=>
    int(28)
    [2]=>
    int(39)
  }
}
array(3) {
  [0]=>
  array(3) {
    [0]=>
    int(101)
    [1]=>
    int(102)
    [2]=>
    int(103)
  }
  [1]=>
  array(3) {
    [0]=>
    int(204)
    [1]=>
    int(205)
    [2]=>
    int(206)
  }
  [2]=>
  array(3) {
    [0]=>
    int(307)
    [1]=>
    int(308)
    [2]=>
    int(309)
  }
}
array(2) {
  [0]=>
  array(2) {
    [0]=>
    array(2) {
      [0]=>
      int(11)
      [1]=>
      int(22)
    }
    [1]=>
    array(2) {
      [0]=>
      int(13)
      [1]=>
      int(24)
    }
  }
  [1]=>
  array(2) {
    [0]=>
    array(2) {
      [0]=>
      int(15)
      [1]=>
      int(26)
    }
    [1]=>
    array(2) {
      [0]=>
      int(17)
      [1]=>
      int(28)
    }
  }
}
array(3) {
  [0]=>
  array(3) {
    [0]=>
    int(101)
    [1]=>
    int(102)
    [2]=>
    int(103)
  }
  [1]=>
  array(3) {
    [0]=>
    int(104)
    [1]=>
    int(105)
    [2]=>
    int(106)
  }
  [2]=>
  array(3) {
    [0]=>
    int(107)
    [1]=>
    int(108)
    [2]=>
    int(109)
  }
}
