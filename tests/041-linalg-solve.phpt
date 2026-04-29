--TEST--
Linalg::solve — A @ x == b for known x; multiple RHS; singular A throws
--FILE--
<?php
function close_vec($a, $expected, $tol = 1e-10) {
    $av = $a->toArray();
    foreach ($expected as $i => $e) if (abs($av[$i] - $e) > $tol) return false;
    return true;
}
function close_mat($a, $expected, $tol = 1e-10) {
    $av = $a->toArray();
    foreach ($expected as $i => $row) {
        foreach ($row as $j => $e) if (abs($av[$i][$j] - $e) > $tol) return false;
    }
    return true;
}

// Symmetric A — sanity baseline
$a_sym = NDArray::fromArray([[2.0, 1.0], [1.0, 3.0]]);
$b_sym = NDArray::fromArray([10.0, 15.0]);  // A @ [3, 4] = [10, 15]
echo close_vec(Linalg::solve($a_sym, $b_sym), [3.0, 4.0]) ? "sym-ok\n" : "sym-FAIL\n";

// Asymmetric A — catches transpose-trick bug
$a_asym = NDArray::fromArray([[4.0, 7.0], [2.0, 6.0]]);
// A @ [1, 2] = [4 + 14, 2 + 12] = [18, 14]
$b_asym = NDArray::fromArray([18.0, 14.0]);
echo close_vec(Linalg::solve($a_asym, $b_asym), [1.0, 2.0]) ? "asym-ok\n" : "asym-FAIL\n";

// 3x3 asymmetric — the strict transpose-trick test
$a3 = NDArray::fromArray([
    [4.0, 2.0, 1.0],
    [3.0, 5.0, 1.0],
    [1.0, 1.0, 3.0],
]);
$x_known = [1.0, -1.0, 2.0];
// b = A @ x = [4 - 2 + 2, 3 - 5 + 2, 1 - 1 + 6] = [4, 0, 6]
$b3 = NDArray::fromArray([4.0, 0.0, 6.0]);
echo close_vec(Linalg::solve($a3, $b3), $x_known) ? "3x3-ok\n" : "3x3-FAIL\n";

// Multiple RHS — b is 2D shape (n, nrhs)
//   A = [[2, 1], [1, 3]]
//   x = [[3, 1], [4, 2]]   (two solution vectors)
//   b = A @ x = [[10, 4], [15, 7]]
$b_multi = NDArray::fromArray([[10.0, 4.0], [15.0, 7.0]]);
echo close_mat(Linalg::solve($a_sym, $b_multi), [[3.0, 1.0], [4.0, 2.0]]) ? "multi-rhs-ok\n" : "multi-rhs-FAIL\n";

// Singular A throws
$sing = NDArray::fromArray([[1.0, 2.0], [2.0, 4.0]]);
$b = NDArray::fromArray([1.0, 1.0]);
try { Linalg::solve($sing, $b); echo "FAIL\n"; }
catch (NDArrayException $e) { echo "singular\n"; }

// Shape mismatch (A is 3x3, b is length 2)
try { Linalg::solve($a3, NDArray::fromArray([1.0, 2.0])); echo "FAIL\n"; }
catch (ShapeException $e) { echo "shape-mismatch\n"; }

// A non-square throws
try { Linalg::solve(NDArray::fromArray([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]), $b); echo "FAIL\n"; }
catch (ShapeException $e) { echo "non-square\n"; }
?>
--EXPECT--
sym-ok
asym-ok
3x3-ok
multi-rhs-ok
singular
shape-mismatch
non-square
