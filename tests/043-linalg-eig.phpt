--TEST--
Linalg::eig — A @ v == λ v; complex eigenvalues throw
--FILE--
<?php
function close_vec($a, $expected, $tol = 1e-9) {
    $av = $a->toArray();
    foreach ($expected as $i => $e) if (abs($av[$i] - $e) > $tol) return false;
    return true;
}
function sort_floats(array $a) { sort($a); return $a; }

// Diagonal — eigenvalues are the diagonal entries
$diag = NDArray::fromArray([[2.0, 0.0], [0.0, 3.0]]);
[$w, $V] = Linalg::eig($diag);
$wv = sort_floats($w->toArray());
echo close_vec(NDArray::fromArray($wv), [2.0, 3.0]) ? "diag-eigvals\n" : "diag-FAIL\n";

// Symmetric 2x2 [[2, 1], [1, 2]] → eigenvalues are 1 and 3
$sym = NDArray::fromArray([[2.0, 1.0], [1.0, 2.0]]);
[$w, $V] = Linalg::eig($sym);
$wv = sort_floats($w->toArray());
echo close_vec(NDArray::fromArray($wv), [1.0, 3.0]) ? "sym2-eigvals\n" : "sym2-FAIL\n";

// Verify A @ v_i ≈ λ_i v_i for each eigenvector column
$wraw = $w->toArray();
$Vraw = $V->toArray();
$ok = true;
for ($i = 0; $i < 2; $i++) {
    $vi = NDArray::fromArray([$Vraw[0][$i], $Vraw[1][$i]]);
    $lhs = NDArray::matmul($sym, $vi->reshape([2, 1]))->reshape([2])->toArray();
    $rhs = [$wraw[$i] * $Vraw[0][$i], $wraw[$i] * $Vraw[1][$i]];
    foreach ($lhs as $k => $v) if (abs($v - $rhs[$k]) > 1e-9) $ok = false;
}
echo $ok ? "Av-eq-lambda-v\n" : "Av-eq-lambda-v-FAIL\n";

// Non-symmetric upper-triangular [[2, 1], [0, 3]] → eigenvalues are 2 and 3
$tri = NDArray::fromArray([[2.0, 1.0], [0.0, 3.0]]);
[$w, $V] = Linalg::eig($tri);
$wv = sort_floats($w->toArray());
echo close_vec(NDArray::fromArray($wv), [2.0, 3.0]) ? "tri-eigvals\n" : "tri-FAIL\n";

// Rotation matrix [[0, -1], [1, 0]] — eigenvalues ±i, MUST throw (no complex dtype in v1)
$rot = NDArray::fromArray([[0.0, -1.0], [1.0, 0.0]]);
try {
    Linalg::eig($rot);
    echo "complex-FAIL (should have thrown)\n";
} catch (NDArrayException $e) {
    echo "complex-throws\n";
    // Message should mention complex eigenvalues
    if (strpos($e->getMessage(), "complex") !== false) echo "msg-mentions-complex\n";
}

// Non-square throws
try { Linalg::eig(NDArray::fromArray([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]])); echo "FAIL\n"; }
catch (ShapeException $e) { echo "non-square\n"; }

// 1-D throws
try { Linalg::eig(NDArray::fromArray([1.0, 2.0])); echo "FAIL\n"; }
catch (ShapeException $e) { echo "1d\n"; }
?>
--EXPECT--
diag-eigvals
sym2-eigvals
Av-eq-lambda-v
tri-eigvals
complex-throws
msg-mentions-complex
non-square
1d
