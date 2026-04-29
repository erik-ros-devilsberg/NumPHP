--TEST--
Linalg::norm — vector / matrix norms
--FILE--
<?php
$tol = 1e-12;

// Vector 2-norm (Euclidean): |[3, 4]| = 5
$v = NDArray::fromArray([3.0, 4.0]);
echo abs(Linalg::norm($v) - 5.0) < $tol ? "v2-ok\n" : "v2-FAIL\n";
echo abs(Linalg::norm($v, 2) - 5.0) < $tol ? "v2-explicit-ok\n" : "v2-explicit-FAIL\n";

// Vector 1-norm: sum of |x|: |3| + |-4| = 7
echo abs(Linalg::norm(NDArray::fromArray([3.0, -4.0]), 1) - 7.0) < $tol ? "v1-ok\n" : "v1-FAIL\n";

// Vector inf-norm: max |x|: max(3, 4, 1) = 4
echo abs(Linalg::norm(NDArray::fromArray([3.0, -4.0, 1.0]), INF) - 4.0) < $tol ? "vinf-ok\n" : "vinf-FAIL\n";

// Matrix Frobenius (= sqrt(sum x^2)): for [[1,2],[3,4]] → sqrt(30)
$m = NDArray::fromArray([[1.0, 2.0], [3.0, 4.0]]);
echo abs(Linalg::norm($m, 'fro') - sqrt(30.0)) < $tol ? "mfro-ok\n" : "mfro-FAIL\n";

// Default for 2-D — match NumPy: ord=2 on a matrix is the spectral norm,
// but v1 substitutes Frobenius. Document and accept.
echo abs(Linalg::norm($m) - sqrt(30.0)) < $tol ? "mfro-default-ok\n" : "mfro-default-FAIL\n";

// Matrix 1-norm: max column sum. [[1,2],[3,4]] → max(1+3, 2+4) = 6
echo abs(Linalg::norm($m, 1) - 6.0) < $tol ? "m1-ok\n" : "m1-FAIL\n";

// Matrix inf-norm: max row sum. [[1,2],[3,4]] → max(1+2, 3+4) = 7
echo abs(Linalg::norm($m, INF) - 7.0) < $tol ? "minf-ok\n" : "minf-FAIL\n";

// Vector 2-norm along axis 0 of a matrix:
//   axis=0 collapses rows → [|col0|, |col1|]
//   col0 = [1, 3] → sqrt(10), col1 = [2, 4] → sqrt(20)
$ax0 = Linalg::norm($m, 2, 0);
$av = $ax0->toArray();
echo (abs($av[0] - sqrt(10.0)) < $tol && abs($av[1] - sqrt(20.0)) < $tol) ? "axis0-ok\n" : "axis0-FAIL\n";

// axis=1 → [|row0|, |row1|] = [sqrt(5), sqrt(25)] = [sqrt(5), 5]
$ax1 = Linalg::norm($m, 2, 1);
$av = $ax1->toArray();
echo (abs($av[0] - sqrt(5.0)) < $tol && abs($av[1] - 5.0) < $tol) ? "axis1-ok\n" : "axis1-FAIL\n";

// Unsupported ord throws
try { Linalg::norm($v, 'bogus'); echo "FAIL\n"; }
catch (NDArrayException $e) { echo "bad-ord\n"; }

// Zero vector → 0
echo abs(Linalg::norm(NDArray::fromArray([0.0, 0.0, 0.0]))) < $tol ? "zero-ok\n" : "zero-FAIL\n";
?>
--EXPECT--
v2-ok
v2-explicit-ok
v1-ok
vinf-ok
mfro-ok
mfro-default-ok
m1-ok
minf-ok
axis0-ok
axis1-ok
bad-ord
zero-ok
