--TEST--
Linalg::det — known dets, sign, singular
--FILE--
<?php
$tol = 1e-10;

// Identity → 1
echo abs(Linalg::det(NDArray::eye(3)) - 1.0) < $tol ? "id-ok\n" : "id-FAIL\n";

// Diagonal → product of diagonal: diag(2, 3, 5) → 30
$d = NDArray::fromArray([[2.0, 0.0, 0.0], [0.0, 3.0, 0.0], [0.0, 0.0, 5.0]]);
echo abs(Linalg::det($d) - 30.0) < $tol ? "diag-ok\n" : "diag-FAIL\n";

// 2x2 [[4, 7], [2, 6]] → 4*6 - 7*2 = 10
$a2 = NDArray::fromArray([[4.0, 7.0], [2.0, 6.0]]);
echo abs(Linalg::det($a2) - 10.0) < $tol ? "2x2-ok\n" : "2x2-FAIL: " . Linalg::det($a2) . "\n";

// Asymmetric 3x3 — det was hand-computed:
//   [[4, 2, 1], [3, 5, 1], [1, 1, 3]]
//   det = 4*(5*3 - 1*1) - 2*(3*3 - 1*1) + 1*(3*1 - 5*1)
//       = 4*14 - 2*8 + 1*(-2) = 56 - 16 - 2 = 38
$a3 = NDArray::fromArray([
    [4.0, 2.0, 1.0],
    [3.0, 5.0, 1.0],
    [1.0, 1.0, 3.0],
]);
echo abs(Linalg::det($a3) - 38.0) < $tol ? "3x3-ok\n" : "3x3-FAIL: " . Linalg::det($a3) . "\n";

// Sign: swap two rows of identity → det = -1
$swap = NDArray::fromArray([[0.0, 1.0, 0.0], [1.0, 0.0, 0.0], [0.0, 0.0, 1.0]]);
echo abs(Linalg::det($swap) + 1.0) < $tol ? "sign-ok\n" : "sign-FAIL: " . Linalg::det($swap) . "\n";

// Singular → 0
$sing = NDArray::fromArray([[1.0, 2.0], [2.0, 4.0]]);
echo abs(Linalg::det($sing)) < 1e-9 ? "singular-ok\n" : "singular-FAIL\n";

// Returns float
var_dump(is_float(Linalg::det($a2)));                   // true

// Non-square throws
try { Linalg::det(NDArray::fromArray([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]])); echo "FAIL\n"; }
catch (ShapeException $e) { echo "non-square\n"; }
?>
--EXPECT--
id-ok
diag-ok
2x2-ok
3x3-ok
sign-ok
singular-ok
bool(true)
non-square
