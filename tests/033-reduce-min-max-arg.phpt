--TEST--
min / max / argmin / argmax — value and index reductions
--FILE--
<?php
$a = NDArray::fromArray([[3, 1, 4], [1, 5, 9]]);

// Global value reductions
var_dump($a->min());                              // int(1)
var_dump($a->max());                              // int(9)

// Global arg reductions — returns int (flat index, first occurrence)
var_dump($a->argmin());                           // int(1)  -> position of first 1 in flat
var_dump($a->argmax());                           // int(5)  -> position of 9 in flat

// Axis reductions
var_dump($a->min(0)->toArray());                  // [1, 1, 4]
var_dump($a->max(0)->toArray());                  // [3, 5, 9]
var_dump($a->min(1)->toArray());                  // [1, 1]
var_dump($a->max(1)->toArray());                  // [4, 9]

// argmin/argmax along axis returns int64
$am0 = $a->argmin(0);
var_dump($am0->dtype());                          // int64
var_dump($am0->toArray());                        // [1, 0, 0]   -> ties resolved to first occurrence
var_dump($a->argmax(0)->toArray());               // [0, 1, 1]
var_dump($a->argmin(1)->toArray());               // [1, 0]
var_dump($a->argmax(1)->toArray());               // [2, 2]

// keepdims
var_dump($a->min(0, true)->shape());              // [1, 3]
var_dump($a->argmax(1, true)->shape());           // [2, 1]

// Float dtype with NaN — min/max propagate NaN
$f = NDArray::fromArray([1.0, NAN, 2.0, 0.5]);
var_dump(is_nan($f->min()));                      // true (NaN propagates)
var_dump(is_nan($f->max()));                      // true
?>
--EXPECT--
int(1)
int(9)
int(1)
int(5)
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(1)
  [2]=>
  int(4)
}
array(3) {
  [0]=>
  int(3)
  [1]=>
  int(5)
  [2]=>
  int(9)
}
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(1)
}
array(2) {
  [0]=>
  int(4)
  [1]=>
  int(9)
}
string(5) "int64"
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(0)
  [2]=>
  int(0)
}
array(3) {
  [0]=>
  int(0)
  [1]=>
  int(1)
  [2]=>
  int(1)
}
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(0)
}
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(2)
}
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(3)
}
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(1)
}
bool(true)
bool(true)
