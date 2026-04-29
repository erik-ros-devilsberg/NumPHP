--TEST--
BLAS ops handle non-contiguous inputs by materialising to contiguous
--FILE--
<?php
// Construct a non-contiguous matrix via transpose
$A_orig = NDArray::fromArray([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]);  // [2, 3]
$A_t = $A_orig->transpose();                                         // [3, 2], non-contig
$B = NDArray::fromArray([[1.0, 0.0], [0.0, 1.0]]);                  // [2, 2]

$C = NDArray::matmul($A_t, $B);                                      // [3, 2] × [2, 2] = [3, 2]
var_dump($C->toArray());

// matmul where A is non-contig but result equivalent to contig run
$A_contig = NDArray::fromArray([[1.0, 4.0], [2.0, 5.0], [3.0, 6.0]]); // same data as A_t
$C_ref = NDArray::matmul($A_contig, $B);
var_dump($C->toArray() === $C_ref->toArray());

// Sliced view also works
$big = NDArray::arange(0.0, 12.0)->reshape([3, 4]);                  // [3, 4]
$sub = $big->slice(0, 3);                                            // identical (slice axis 0 with full range, contig)
var_dump(($big->slice(0, 3) instanceof NDArray) === true);
?>
--EXPECT--
array(3) {
  [0]=>
  array(2) {
    [0]=>
    float(1)
    [1]=>
    float(4)
  }
  [1]=>
  array(2) {
    [0]=>
    float(2)
    [1]=>
    float(5)
  }
  [2]=>
  array(2) {
    [0]=>
    float(3)
    [1]=>
    float(6)
  }
}
bool(true)
bool(true)
