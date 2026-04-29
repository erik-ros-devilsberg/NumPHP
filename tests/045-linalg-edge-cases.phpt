--TEST--
Linalg edge cases — non-contig input, dtype dispatch, layout independence
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

// Non-contiguous input: transpose-of-A is a view (F-contiguous, not C).
// inv(A^T) should equal inv(A)^T.
$a = NDArray::fromArray([[4.0, 7.0], [2.0, 6.0]]);
$inv_a = Linalg::inv($a);
$at = $a->transpose();                      // view, non-C-contiguous
$inv_at = Linalg::inv($at);                 // must materialise via ensure_contig_dtype
$inv_a_T = $inv_a->transpose();
echo close_to($inv_at, $inv_a_T) ? "transpose-view-ok\n" : "transpose-view-FAIL\n";

// f32 dispatch: input f32 → output f32 across all six ops (where applicable)
$af32 = NDArray::eye(2, null, 0, 'float32');
var_dump(Linalg::inv($af32)->dtype());                              // float32
var_dump(is_float(Linalg::det($af32)));                             // det always float
[$U, $S, $Vt] = Linalg::svd($af32);
var_dump($U->dtype());                                              // float32
var_dump($S->dtype());                                              // float32
var_dump($Vt->dtype());                                             // float32

// int input → f64 output
$ai = NDArray::fromArray([[2, 0], [0, 3]]);
var_dump(Linalg::inv($ai)->dtype());                                // float64
[$Ui, $Si, $Vti] = Linalg::svd($ai);
var_dump($Ui->dtype());                                             // float64

// Singular detection via det == 0 (sanity)
$s = NDArray::fromArray([[1.0, 2.0], [2.0, 4.0]]);
echo abs(Linalg::det($s)) < 1e-9 ? "sing-det-zero\n" : "sing-det-FAIL\n";

// LAPACK info code surfaces in exception message
$sing_b = NDArray::fromArray([1.0, 1.0]);
try { Linalg::solve($s, $sing_b); echo "FAIL\n"; }
catch (NDArrayException $e) {
    $msg = $e->getMessage();
    echo (strpos($msg, "info") !== false || strpos($msg, "singular") !== false) ? "info-in-msg\n" : "no-info: $msg\n";
}

// Layout independence: solve via inv (slower) vs solve directly — should agree
$x_via_inv = NDArray::matmul(Linalg::inv($a), NDArray::fromArray([18.0, 14.0])->reshape([2, 1]));
$x_direct  = Linalg::solve($a, NDArray::fromArray([18.0, 14.0]));
echo (abs($x_via_inv->toArray()[0][0] - $x_direct->toArray()[0]) < 1e-9
   && abs($x_via_inv->toArray()[1][0] - $x_direct->toArray()[1]) < 1e-9) ? "layout-indep-ok\n" : "layout-indep-FAIL\n";
?>
--EXPECT--
transpose-view-ok
string(7) "float32"
bool(true)
string(7) "float32"
string(7) "float32"
string(7) "float32"
string(7) "float64"
string(7) "float64"
sing-det-zero
info-in-msg
layout-indep-ok
