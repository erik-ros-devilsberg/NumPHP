--TEST--
ArrayAccess offsetSet — scalar, nD broadcast, NDArray same-shape, errors
--FILE--
<?php
$a = NDArray::zeros([4], 'int64');
$a[0] = 10;
$a[2] = 99;
$a[-1] = 7;
var_dump($a->toArray());

$b = NDArray::zeros([2, 3], 'int64');
$b[0] = 5;                          // scalar broadcast over row
var_dump($b->toArray());

$row = NDArray::fromArray([100, 200, 300]);
$b[1] = $row;                       // shape-match assign
var_dump($b->toArray());

// chained set: $b[0][1] = ...
$b[0][1] = 42;
var_dump($b->toArray());

// shape mismatch
try {
    $b[0] = NDArray::fromArray([1, 2]);
} catch (ShapeException $e) {
    echo "shape-mismatch: ", $e->getMessage(), "\n";
}

// PHP-array RHS deferred
try {
    $b[0] = [1, 2, 3];
} catch (NDArrayException $e) {
    echo "phparray-rhs-deferred: ", $e->getMessage(), "\n";
}

// append rejected
try {
    $a[] = 1;
} catch (IndexException $e) {
    echo "append-rejected: ", $e->getMessage(), "\n";
}

// unset rejected
try {
    unset($a[0]);
} catch (NDArrayException $e) {
    echo "unset-rejected: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
array(4) {
  [0]=>
  int(10)
  [1]=>
  int(0)
  [2]=>
  int(99)
  [3]=>
  int(7)
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    int(5)
    [1]=>
    int(5)
    [2]=>
    int(5)
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
  array(3) {
    [0]=>
    int(5)
    [1]=>
    int(5)
    [2]=>
    int(5)
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
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    int(5)
    [1]=>
    int(42)
    [2]=>
    int(5)
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
}
shape-mismatch: Shape mismatch in assignment
phparray-rhs-deferred: Assignment value must be a scalar or NDArray (PHP-array RHS deferred to a later sprint)
append-rejected: Appending to NDArray ($a[] = $x) is not supported
unset-rejected: NDArray does not support unset; use slicing or recreate the array
