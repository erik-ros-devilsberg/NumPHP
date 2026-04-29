--TEST--
NDArray::reshape — view when contiguous, copy when not, -1 inference
--FILE--
<?php
$a = NDArray::arange(0, 12);
var_dump($a->reshape([3, 4])->toArray());
var_dump($a->reshape([2, -1, 2])->toArray());     // infer middle dim → 3

// reshape after transpose (non-contiguous source) → copy
$b = NDArray::fromArray([[1, 2, 3], [4, 5, 6]]);
$bt = $b->transpose();                              // shape [3, 2], non-contig
var_dump($bt->toArray());
var_dump($bt->reshape([6])->toArray());             // copy + flat

// total-size mismatch
try { $a->reshape([5, 3]); } catch (ShapeException $e) { echo "size-mismatch: ", $e->getMessage(), "\n"; }

// two -1
try { $a->reshape([-1, -1]); } catch (ShapeException $e) { echo "two-neg-one: ", $e->getMessage(), "\n"; }

// non-divisible inference
try { $a->reshape([5, -1]); } catch (ShapeException $e) { echo "non-divisible: ", $e->getMessage(), "\n"; }
?>
--EXPECT--
array(3) {
  [0]=>
  array(4) {
    [0]=>
    int(0)
    [1]=>
    int(1)
    [2]=>
    int(2)
    [3]=>
    int(3)
  }
  [1]=>
  array(4) {
    [0]=>
    int(4)
    [1]=>
    int(5)
    [2]=>
    int(6)
    [3]=>
    int(7)
  }
  [2]=>
  array(4) {
    [0]=>
    int(8)
    [1]=>
    int(9)
    [2]=>
    int(10)
    [3]=>
    int(11)
  }
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    array(2) {
      [0]=>
      int(0)
      [1]=>
      int(1)
    }
    [1]=>
    array(2) {
      [0]=>
      int(2)
      [1]=>
      int(3)
    }
    [2]=>
    array(2) {
      [0]=>
      int(4)
      [1]=>
      int(5)
    }
  }
  [1]=>
  array(3) {
    [0]=>
    array(2) {
      [0]=>
      int(6)
      [1]=>
      int(7)
    }
    [1]=>
    array(2) {
      [0]=>
      int(8)
      [1]=>
      int(9)
    }
    [2]=>
    array(2) {
      [0]=>
      int(10)
      [1]=>
      int(11)
    }
  }
}
array(3) {
  [0]=>
  array(2) {
    [0]=>
    int(1)
    [1]=>
    int(4)
  }
  [1]=>
  array(2) {
    [0]=>
    int(2)
    [1]=>
    int(5)
  }
  [2]=>
  array(2) {
    [0]=>
    int(3)
    [1]=>
    int(6)
  }
}
array(6) {
  [0]=>
  int(1)
  [1]=>
  int(4)
  [2]=>
  int(2)
  [3]=>
  int(5)
  [4]=>
  int(3)
  [5]=>
  int(6)
}
size-mismatch: reshape: total size mismatch (15 vs 12)
two-neg-one: reshape: only one -1 placeholder allowed
non-divisible: reshape: total size 12 not divisible by known dims product 5
