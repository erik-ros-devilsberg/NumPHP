--TEST--
logicalAnd / Or / Xor / Not — any input, output always bool
--FILE--
<?php
/* ===== bool ⊕ bool ===== */

$t = NDArray::fromArray([true, true, false, false], 'bool');
$f = NDArray::fromArray([true, false, true, false], 'bool');

var_dump(NDArray::logicalAnd($t, $f)->toArray());     // [true, false, false, false]
var_dump(NDArray::logicalOr($t, $f)->toArray());      // [true, true, true, false]
var_dump(NDArray::logicalXor($t, $f)->toArray());     // [false, true, true, false]

echo NDArray::logicalAnd($t, $f)->dtype() === 'bool' ? "and-bool-dtype-ok\n" : "and-bool-dtype-FAIL\n";

/* ===== int input — coerced to bool, output is bool ===== */

$a = NDArray::fromArray([0, 1, 2, 3], 'int32');
$b = NDArray::fromArray([5, 0, 7, 0], 'int32');

var_dump(NDArray::logicalAnd($a, $b)->toArray());     // [false, false, true, false]
var_dump(NDArray::logicalOr($a, $b)->toArray());      // [true, true, true, true]
var_dump(NDArray::logicalXor($a, $b)->toArray());     // [true, true, false, true]

echo NDArray::logicalAnd($a, $b)->dtype() === 'bool' ? "and-int-out-bool-ok\n" : "and-int-out-bool-FAIL\n";

/* ===== float input ===== */

$fa = NDArray::fromArray([0.0, 1.5, 0.0, NAN]);
$fb = NDArray::fromArray([2.0, 0.0, 0.0, 1.0]);

// NaN is truthy (matches NumPy / (bool)NAN)
var_dump(NDArray::logicalAnd($fa, $fb)->toArray());   // [false, false, false, true]
var_dump(NDArray::logicalOr($fa, $fb)->toArray());    // [true, true, false, true]

echo NDArray::logicalAnd($fa, $fb)->dtype() === 'bool' ? "and-f64-out-bool-ok\n" : "and-f64-out-bool-FAIL\n";

/* ===== mixed dtype input — output still bool ===== */

$mix = NDArray::logicalAnd($t, $a);
echo $mix->dtype() === 'bool' ? "mixed-out-bool-ok\n" : "mixed-out-bool-FAIL\n";
var_dump($mix->toArray());                            // [false, true, false, false]

/* ===== scalar ⊕ NDArray ===== */

var_dump(NDArray::logicalAnd($a, true)->toArray());   // [false, true, true, true]
var_dump(NDArray::logicalAnd(0, $a)->toArray());      // [false, false, false, false]
var_dump(NDArray::logicalOr(NAN, $a)->toArray());     // [true, true, true, true]

/* ===== broadcasting ===== */

$col = NDArray::fromArray([1, 0, 1], 'int32');
$row = NDArray::fromArray([[1], [0]], 'int32');
$bc  = NDArray::logicalOr($col, $row);
var_dump($bc->shape());                               // [2, 3]
var_dump($bc->toArray());                             // [[true,true,true],[true,false,true]]

/* ===== logicalNot ===== */

// Bool input
$nb = NDArray::logicalNot($t);
echo $nb->dtype() === 'bool' ? "not-bool-dtype-ok\n" : "not-bool-dtype-FAIL\n";
var_dump($nb->toArray());                             // [false, false, true, true]

// Int input
$ni = NDArray::logicalNot($a);
echo $ni->dtype() === 'bool' ? "not-int-dtype-ok\n" : "not-int-dtype-FAIL\n";
var_dump($ni->toArray());                             // [true, false, false, false]

// Float incl. NaN
$nf = NDArray::logicalNot($fa);
echo $nf->dtype() === 'bool' ? "not-f64-dtype-ok\n" : "not-f64-dtype-FAIL\n";
var_dump($nf->toArray());                             // [true, false, true, false] — NaN coerces to true, then negated

/* ===== empty arrays ===== */

$e0 = NDArray::zeros([0], 'int32');
var_dump(NDArray::logicalAnd($e0, $e0)->shape());     // [0]
echo NDArray::logicalAnd($e0, $e0)->dtype() === 'bool' ? "empty-out-bool-ok\n" : "empty-out-bool-FAIL\n";

/* ===== shape mismatch ===== */

try {
    NDArray::logicalAnd(
        NDArray::fromArray([true, false, true], 'bool'),
        NDArray::fromArray([true, false], 'bool')
    );
    echo "shape-mismatch-no-throw-FAIL\n";
} catch (\ShapeException $e) {
    echo "shape-mismatch-throws-ok\n";
}
?>
--EXPECT--
array(4) {
  [0]=>
  bool(true)
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
  bool(true)
  [2]=>
  bool(true)
  [3]=>
  bool(false)
}
array(4) {
  [0]=>
  bool(false)
  [1]=>
  bool(true)
  [2]=>
  bool(true)
  [3]=>
  bool(false)
}
and-bool-dtype-ok
array(4) {
  [0]=>
  bool(false)
  [1]=>
  bool(false)
  [2]=>
  bool(true)
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
  bool(true)
  [1]=>
  bool(true)
  [2]=>
  bool(false)
  [3]=>
  bool(true)
}
and-int-out-bool-ok
array(4) {
  [0]=>
  bool(false)
  [1]=>
  bool(false)
  [2]=>
  bool(false)
  [3]=>
  bool(true)
}
array(4) {
  [0]=>
  bool(true)
  [1]=>
  bool(true)
  [2]=>
  bool(false)
  [3]=>
  bool(true)
}
and-f64-out-bool-ok
mixed-out-bool-ok
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
  bool(true)
  [2]=>
  bool(true)
  [3]=>
  bool(true)
}
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(3)
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    bool(true)
    [1]=>
    bool(true)
    [2]=>
    bool(true)
  }
  [1]=>
  array(3) {
    [0]=>
    bool(true)
    [1]=>
    bool(false)
    [2]=>
    bool(true)
  }
}
not-bool-dtype-ok
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
not-int-dtype-ok
array(4) {
  [0]=>
  bool(true)
  [1]=>
  bool(false)
  [2]=>
  bool(false)
  [3]=>
  bool(false)
}
not-f64-dtype-ok
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
array(1) {
  [0]=>
  int(0)
}
empty-out-bool-ok
shape-mismatch-throws-ok
