--TEST--
Comparison ops — eq/ne/lt/le/gt/ge with broadcast, promotion, NaN
--FILE--
<?php
$a = NDArray::fromArray([1.0, 2.0, 3.0, 4.0]);
$b = NDArray::fromArray([1.0, 2.5, 3.0, 4.0]);

// eq / ne
var_dump(NDArray::eq($a, $b)->dtype());                // bool
var_dump(NDArray::eq($a, $b)->toArray());              // [t, f, t, t]
var_dump(NDArray::ne($a, $b)->toArray());              // [f, t, f, f]

// lt / le / gt / ge
var_dump(NDArray::lt($a, $b)->toArray());              // [f, t, f, f]
var_dump(NDArray::le($a, $b)->toArray());              // [t, t, t, t]
var_dump(NDArray::gt($a, $b)->toArray());              // [f, f, f, f]
var_dump(NDArray::ge($a, $b)->toArray());              // [t, f, t, t]

// Scalar operand on RHS
var_dump(NDArray::gt($a, 2)->toArray());               // [f, f, t, t]
var_dump(NDArray::le(0.5, $a)->toArray());             // [t, t, t, t]   scalar LHS

// Mixed dtype — int32 vs float64; promote-then-compare
$i = NDArray::fromArray([1, 2, 3, 4], 'int32');
$f = NDArray::fromArray([1.0, 2.5, 3.0, 4.5], 'float64');
var_dump(NDArray::eq($i, $f)->toArray());              // [t, f, t, f]

// Broadcasting — col vs row
$col = NDArray::fromArray([[0.0], [1.0], [2.0]]);      // 3x1
$row = NDArray::fromArray([0.0, 1.0]);                 // 2 → broadcasts to 3x2
$bc = NDArray::ge($col, $row);
var_dump($bc->shape());                                // [3, 2]
var_dump($bc->toArray());

// NaN policy (decision 33)
$nan = NDArray::fromArray([NAN, NAN, 1.0]);
$one = NDArray::fromArray([NAN, 1.0, NAN]);
var_dump(NDArray::eq($nan, $one)->toArray());          // [f, f, f]
var_dump(NDArray::ne($nan, $one)->toArray());          // [t, t, t]   IEEE: NaN-anything !=
var_dump(NDArray::lt($nan, $one)->toArray());          // [f, f, f]
var_dump(NDArray::gt($nan, $one)->toArray());          // [f, f, f]
var_dump(NDArray::le($nan, $one)->toArray());          // [f, f, f]
var_dump(NDArray::ge($nan, $one)->toArray());          // [f, f, f]

// Bool-input comparisons (after Phase 2 — bool reads as 0/1)
$bA = NDArray::fromArray([true, false, true], 'bool');
$bB = NDArray::fromArray([true, true, false], 'bool');
var_dump(NDArray::eq($bA, $bB)->toArray());            // [t, f, f]

// Shape mismatch
try {
    NDArray::eq(
        NDArray::fromArray([1, 2, 3]),
        NDArray::fromArray([1, 2]));
    echo "FAIL\n";
} catch (ShapeException $e) {
    echo "shape-mismatch-ok\n";
}
?>
--EXPECT--
string(4) "bool"
array(4) {
  [0]=>
  bool(true)
  [1]=>
  bool(false)
  [2]=>
  bool(true)
  [3]=>
  bool(true)
}
array(4) {
  [0]=>
  bool(false)
  [1]=>
  bool(true)
  [2]=>
  bool(false)
  [3]=>
  bool(false)
}
array(4) {
  [0]=>
  bool(false)
  [1]=>
  bool(true)
  [2]=>
  bool(false)
  [3]=>
  bool(false)
}
array(4) {
  [0]=>
  bool(true)
  [1]=>
  bool(true)
  [2]=>
  bool(true)
  [3]=>
  bool(true)
}
array(4) {
  [0]=>
  bool(false)
  [1]=>
  bool(false)
  [2]=>
  bool(false)
  [3]=>
  bool(false)
}
array(4) {
  [0]=>
  bool(true)
  [1]=>
  bool(false)
  [2]=>
  bool(true)
  [3]=>
  bool(true)
}
array(4) {
  [0]=>
  bool(false)
  [1]=>
  bool(false)
  [2]=>
  bool(true)
  [3]=>
  bool(true)
}
array(4) {
  [0]=>
  bool(true)
  [1]=>
  bool(true)
  [2]=>
  bool(true)
  [3]=>
  bool(true)
}
array(4) {
  [0]=>
  bool(true)
  [1]=>
  bool(false)
  [2]=>
  bool(true)
  [3]=>
  bool(false)
}
array(2) {
  [0]=>
  int(3)
  [1]=>
  int(2)
}
array(3) {
  [0]=>
  array(2) {
    [0]=>
    bool(true)
    [1]=>
    bool(false)
  }
  [1]=>
  array(2) {
    [0]=>
    bool(true)
    [1]=>
    bool(true)
  }
  [2]=>
  array(2) {
    [0]=>
    bool(true)
    [1]=>
    bool(true)
  }
}
array(3) {
  [0]=>
  bool(false)
  [1]=>
  bool(false)
  [2]=>
  bool(false)
}
array(3) {
  [0]=>
  bool(true)
  [1]=>
  bool(true)
  [2]=>
  bool(true)
}
array(3) {
  [0]=>
  bool(false)
  [1]=>
  bool(false)
  [2]=>
  bool(false)
}
array(3) {
  [0]=>
  bool(false)
  [1]=>
  bool(false)
  [2]=>
  bool(false)
}
array(3) {
  [0]=>
  bool(false)
  [1]=>
  bool(false)
  [2]=>
  bool(false)
}
array(3) {
  [0]=>
  bool(false)
  [1]=>
  bool(false)
  [2]=>
  bool(false)
}
array(3) {
  [0]=>
  bool(true)
  [1]=>
  bool(false)
  [2]=>
  bool(false)
}
shape-mismatch-ok
