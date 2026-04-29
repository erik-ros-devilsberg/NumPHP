--TEST--
Linalg::inv — A @ inv(A) == I (asymmetric inputs catch transpose-trick bugs)
--FILE--
<?php
function close_to($actual, $expected, $tol = 1e-10) {
    $a = $actual->toArray();
    $e = $expected->toArray();
    $f_a = []; array_walk_recursive($a, function ($v) use (&$f_a) { $f_a[] = $v; });
    $f_e = []; array_walk_recursive($e, function ($v) use (&$f_e) { $f_e[] = $v; });
    if (count($f_a) !== count($f_e)) return false;
    foreach ($f_a as $i => $v) if (abs($v - $f_e[$i]) > $tol) return false;
    return true;
}

// Asymmetric 2x2 — catches transpose-bug if inv was returned as inv(A^T)
$a2 = NDArray::fromArray([[4.0, 7.0], [2.0, 6.0]]);
$prod2 = NDArray::matmul($a2, Linalg::inv($a2));
echo close_to($prod2, NDArray::eye(2)) ? "2x2-ok\n" : "2x2-FAIL\n";

// Asymmetric 3x3 — the real transpose-trick test
$a3 = NDArray::fromArray([
    [4.0, 2.0, 1.0],
    [3.0, 5.0, 1.0],
    [1.0, 1.0, 3.0],
]);
$inv3 = Linalg::inv($a3);
echo close_to(NDArray::matmul($a3, $inv3), NDArray::eye(3)) ? "3x3-right-ok\n" : "3x3-right-FAIL\n";
echo close_to(NDArray::matmul($inv3, $a3), NDArray::eye(3)) ? "3x3-left-ok\n"  : "3x3-left-FAIL\n";

// f32 path returns f32
$af32 = NDArray::eye(2, null, 0, 'float32');
var_dump(Linalg::inv($af32)->dtype());                  // float32

// int input → f64 output
$ai = NDArray::fromArray([[2, 0], [0, 3]]);
var_dump(Linalg::inv($ai)->dtype());                    // float64

// Singular matrix (row 2 = 2 * row 1)
try { Linalg::inv(NDArray::fromArray([[1.0, 2.0], [2.0, 4.0]])); echo "FAIL\n"; }
catch (NDArrayException $e) { echo "singular\n"; }

// Non-square
try { Linalg::inv(NDArray::fromArray([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]])); echo "FAIL\n"; }
catch (ShapeException $e) { echo "non-square\n"; }

// 1-D
try { Linalg::inv(NDArray::fromArray([1.0, 2.0, 3.0])); echo "FAIL\n"; }
catch (ShapeException $e) { echo "1d\n"; }
?>
--EXPECT--
2x2-ok
3x3-right-ok
3x3-left-ok
string(7) "float32"
string(7) "float64"
singular
non-square
1d
