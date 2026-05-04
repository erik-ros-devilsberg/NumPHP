--TEST--
bitwiseAnd / Or / Xor / Not — int + bool only, float rejected
--FILE--
<?php
/* ===== bool ⊕ bool — output stays bool ===== */

$t = NDArray::fromArray([true, true, false, false], 'bool');
$f = NDArray::fromArray([true, false, true, false], 'bool');

var_dump(NDArray::bitwiseAnd($t, $f)->toArray());     // [true, false, false, false]
var_dump(NDArray::bitwiseOr($t, $f)->toArray());      // [true, true, true, false]
var_dump(NDArray::bitwiseXor($t, $f)->toArray());     // [false, true, true, false]

echo NDArray::bitwiseAnd($t, $f)->dtype() === 'bool' ? "and-bool-dtype-ok\n" : "and-bool-dtype-FAIL\n";

/* ===== int ⊕ int ===== */

$a = NDArray::fromArray([12, 10, 7], 'int32');
$b = NDArray::fromArray([10, 6,  3], 'int32');

var_dump(NDArray::bitwiseAnd($a, $b)->toArray());     // [8, 2, 3]
var_dump(NDArray::bitwiseOr($a, $b)->toArray());      // [14, 14, 7]
var_dump(NDArray::bitwiseXor($a, $b)->toArray());     // [6, 12, 4]

echo NDArray::bitwiseAnd($a, $b)->dtype() === 'int32' ? "and-int32-dtype-ok\n" : "and-int32-dtype-FAIL\n";

/* ===== bool ⊕ int — promotes to int ===== */

$bv = NDArray::fromArray([true, false, true], 'bool');
$iv = NDArray::fromArray([5, 3, 7], 'int32');
$mix = NDArray::bitwiseAnd($bv, $iv);
echo $mix->dtype() === 'int32' ? "mix-promote-dtype-ok\n" : "mix-promote-dtype-FAIL\n";
var_dump($mix->toArray());                            // [1, 0, 1]

/* ===== int32 ⊕ int64 — promotes to int64 ===== */

$i32 = NDArray::fromArray([12, 10], 'int32');
$i64 = NDArray::fromArray([10, 6 ], 'int64');
echo NDArray::bitwiseOr($i32, $i64)->dtype() === 'int64' ? "i32-i64-dtype-ok\n" : "i32-i64-dtype-FAIL\n";

/* ===== scalar ⊕ NDArray ===== */

var_dump(NDArray::bitwiseAnd($a, 6)->toArray());      // [4, 2, 6]
var_dump(NDArray::bitwiseAnd(6, $a)->toArray());      // [4, 2, 6]
// PHP bool scalar wraps as int64 0-D (existing convention in scalar_to_0d_ndarray),
// so bool-array ⊕ scalar-bool promotes to int. The user-visible values are correct.
var_dump(NDArray::bitwiseAnd(true, $bv)->toArray());  // [1, 0, 1] as int

/* ===== broadcasting (1-D vs 2-D) ===== */

$col = NDArray::fromArray([1, 2, 3], 'int32');
$row = NDArray::fromArray([[1], [2]], 'int32');
$bc  = NDArray::bitwiseOr($col, $row);
var_dump($bc->shape());                               // [2, 3]
var_dump($bc->toArray());                             // [[1,3,3],[3,2,3]]

/* ===== bitwiseNot ===== */

// Bool: ~true === false, ~false === true (NumPy match, NOT C-level ~)
$bn = NDArray::bitwiseNot($t);
echo $bn->dtype() === 'bool' ? "not-bool-dtype-ok\n" : "not-bool-dtype-FAIL\n";
var_dump($bn->toArray());                             // [false, false, true, true]

// Int: ~v === -v - 1 (C-level)
$ni = NDArray::bitwiseNot($a);
var_dump($ni->dtype());                               // string(5) "int32"
var_dump($ni->toArray());                             // [-13, -11, -8]

/* ===== float input → \DTypeException ===== */

$ff = NDArray::fromArray([1.0, 2.0]);
try {
    NDArray::bitwiseAnd($ff, $ff);
    echo "and-float-no-throw-FAIL\n";
} catch (\DTypeException $e) {
    echo "and-float-throws: ", $e->getMessage(), "\n";
}
try {
    NDArray::bitwiseOr($a, $ff);
    echo "or-float-mix-no-throw-FAIL\n";
} catch (\DTypeException $e) {
    echo "or-float-mix-throws: ", $e->getMessage(), "\n";
}
try {
    NDArray::bitwiseXor($ff, $a);
    echo "xor-float-mix-no-throw-FAIL\n";
} catch (\DTypeException $e) {
    echo "xor-float-mix-throws: ", $e->getMessage(), "\n";
}
try {
    NDArray::bitwiseNot($ff);
    echo "not-float-no-throw-FAIL\n";
} catch (\DTypeException $e) {
    echo "not-float-throws: ", $e->getMessage(), "\n";
}

/* ===== empty arrays ===== */

$e0 = NDArray::zeros([0], 'int32');
var_dump(NDArray::bitwiseAnd($e0, $e0)->shape());     // [0]
var_dump(NDArray::bitwiseNot($e0)->shape());          // [0]

/* ===== shape mismatch ===== */

try {
    NDArray::bitwiseAnd(
        NDArray::fromArray([1, 2, 3], 'int32'),
        NDArray::fromArray([1, 2], 'int32')
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
array(3) {
  [0]=>
  int(8)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
array(3) {
  [0]=>
  int(14)
  [1]=>
  int(14)
  [2]=>
  int(7)
}
array(3) {
  [0]=>
  int(6)
  [1]=>
  int(12)
  [2]=>
  int(4)
}
and-int32-dtype-ok
mix-promote-dtype-ok
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(0)
  [2]=>
  int(1)
}
i32-i64-dtype-ok
array(3) {
  [0]=>
  int(4)
  [1]=>
  int(2)
  [2]=>
  int(6)
}
array(3) {
  [0]=>
  int(4)
  [1]=>
  int(2)
  [2]=>
  int(6)
}
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(0)
  [2]=>
  int(1)
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
    int(1)
    [1]=>
    int(3)
    [2]=>
    int(3)
  }
  [1]=>
  array(3) {
    [0]=>
    int(3)
    [1]=>
    int(2)
    [2]=>
    int(3)
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
string(5) "int32"
array(3) {
  [0]=>
  int(-13)
  [1]=>
  int(-11)
  [2]=>
  int(-8)
}
and-float-throws: bitwiseAnd: float dtypes not supported (got float64); cast to int first
or-float-mix-throws: bitwiseOr: float dtypes not supported (got float64); cast to int first
xor-float-mix-throws: bitwiseXor: float dtypes not supported (got float64); cast to int first
not-float-throws: bitwiseNot: float dtypes not supported (got float64); cast to int first
array(1) {
  [0]=>
  int(0)
}
array(1) {
  [0]=>
  int(0)
}
shape-mismatch-throws-ok
