--TEST--
sum on int32 promotes to int64 (decision 9 — value-overflow scale)
--FILE--
<?php
// 1 billion × 4 = 4 billion, which overflows int32 max (~2.147 billion) but
// fits cleanly in int64. If the accumulator weren't int64 we'd see a
// wrapped value here. PHP integers are 64-bit on the platforms we support,
// so the visible result type from a full reduction is `integer` carrying
// the correct value.
$a = NDArray::full([4], 1000000000, "int32");
$s = $a->sum();
var_dump(gettype($s));           // string(7) "integer"
var_dump($s);                     // int(4000000000)

// Axis-reduced sum of int32 input must produce int64 NDArray (decision 9).
$m = NDArray::full([2, 4], 1000000000, "int32");
$rsum = $m->sum(1);
echo "axis-1 sum dtype: ", $rsum->dtype(), "\n";   // int64
var_dump($rsum->toArray());

// int64 input stays int64 (no wider promotion target available).
$big = NDArray::full([3], 5, "int64");
echo "int64 sum dtype: ", gettype($big->sum()), "\n";

// Empty int32 array: sum is 0 (additive identity), dtype-preserving.
$empty32 = NDArray::zeros([0], "int32");
$es = $empty32->sum(0, true);
echo "empty int32 sum.dtype: ", $es->dtype(), "\n";  // int64 (promoted)
?>
--EXPECT--
string(7) "integer"
int(4000000000)
axis-1 sum dtype: int64
array(2) {
  [0]=>
  int(4000000000)
  [1]=>
  int(4000000000)
}
int64 sum dtype: integer
empty int32 sum.dtype: int64
