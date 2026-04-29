--TEST--
Linalg::svd — reconstruction U @ diag(S) @ Vt ≈ A (asymmetric + rectangular)
--FILE--
<?php
function close_to($a, $expected, $tol = 1e-9) {
    $av = $a->toArray();
    $ev = $expected->toArray();
    $f_a = []; array_walk_recursive($av, function ($v) use (&$f_a) { $f_a[] = $v; });
    $f_e = []; array_walk_recursive($ev, function ($v) use (&$f_e) { $f_e[] = $v; });
    if (count($f_a) !== count($f_e)) return false;
    foreach ($f_a as $i => $v) if (abs($v - $f_e[$i]) > $tol) return false;
    return true;
}

function reconstruct($U, $S, $Vt) {
    // diag(S) — promote 1-D singular values to a 2-D diagonal of shape (k, k)
    $k = $S->shape()[0];
    $diag = NDArray::zeros([$k, $k]);
    $sv = $S->toArray();
    for ($i = 0; $i < $k; $i++) {
        $row = array_fill(0, $k, 0.0);
        $row[$i] = $sv[$i];
        $diag[$i] = NDArray::fromArray($row);
    }
    return NDArray::matmul(NDArray::matmul($U, $diag), $Vt);
}

// Square symmetric — baseline
$a_sym = NDArray::fromArray([[2.0, 1.0], [1.0, 2.0]]);
[$U, $S, $Vt] = Linalg::svd($a_sym);
echo close_to(reconstruct($U, $S, $Vt), $a_sym) ? "sym-recon\n" : "sym-recon-FAIL\n";

// Square asymmetric — the transpose-trick test
$a_asym = NDArray::fromArray([[4.0, 7.0], [2.0, 6.0]]);
[$U, $S, $Vt] = Linalg::svd($a_asym);
echo close_to(reconstruct($U, $S, $Vt), $a_asym) ? "asym-recon\n" : "asym-recon-FAIL\n";

// Singular values are non-negative and descending
$sv = $S->toArray();
echo ($sv[0] >= $sv[1] && $sv[0] >= 0 && $sv[1] >= 0) ? "sv-order-ok\n" : "sv-order-FAIL\n";

// Rectangular m > n (3 rows, 2 cols)
$tall = NDArray::fromArray([[1.0, 2.0], [3.0, 4.0], [5.0, 6.0]]);
[$U, $S, $Vt] = Linalg::svd($tall);
// Thin SVD: U is (3, 2), S is (2,), Vt is (2, 2)
var_dump($U->shape());
var_dump($S->shape());
var_dump($Vt->shape());
echo close_to(reconstruct($U, $S, $Vt), $tall) ? "tall-recon\n" : "tall-recon-FAIL\n";

// Rectangular m < n (2 rows, 3 cols) — the meaty asymmetric case
$wide = NDArray::fromArray([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]);
[$U, $S, $Vt] = Linalg::svd($wide);
// Thin SVD: U is (2, 2), S is (2,), Vt is (2, 3)
var_dump($U->shape());
var_dump($S->shape());
var_dump($Vt->shape());
echo close_to(reconstruct($U, $S, $Vt), $wide) ? "wide-recon\n" : "wide-recon-FAIL\n";

// Non-2D throws
try { Linalg::svd(NDArray::fromArray([1.0, 2.0])); echo "FAIL\n"; }
catch (ShapeException $e) { echo "1d\n"; }
?>
--EXPECT--
sym-recon
asym-recon
sv-order-ok
array(2) {
  [0]=>
  int(3)
  [1]=>
  int(2)
}
array(1) {
  [0]=>
  int(2)
}
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(2)
}
tall-recon
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(2)
}
array(1) {
  [0]=>
  int(2)
}
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(3)
}
wide-recon
1d
