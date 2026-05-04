--TEST--
prod / nanprod / countNonzero / ptp — multiplicative reduction, NaN-aware, mask counting, peak-to-peak
--FILE--
<?php
/* ===== prod ===== */

// 1-D global
$a = NDArray::fromArray([1, 2, 3, 4]);
var_dump($a->prod());                             // int(24)

// 2-D, axis
$m = NDArray::fromArray([[1, 2, 3], [4, 5, 6]]);
var_dump($m->prod());                             // int(720)
var_dump($m->prod(0)->toArray());                 // [4, 10, 18]
var_dump($m->prod(1)->toArray());                 // [6, 120]
var_dump($m->prod(0, true)->shape());             // [1, 3]
var_dump($m->prod(null, true)->shape());          // [1, 1]

// negative axis
var_dump($m->prod(-1)->toArray());                // [6, 120]

// int → int64 promotion (decision 39, locks divergence from NumPy)
echo $a->prod() === 24 ? "prod-int-ok\n" : "prod-int-FAIL\n";
$ai = NDArray::fromArray([1, 2, 3, 4], 'int32');
echo $ai->prod(0)->dtype() === 'int64' ? "prod-int32-promote-ok\n" : "prod-int32-promote-FAIL\n";
$ai64 = NDArray::fromArray([1, 2, 3, 4], 'int64');
echo $ai64->prod(0)->dtype() === 'int64' ? "prod-int64-stays-ok\n" : "prod-int64-stays-FAIL\n";

// int32 overflow protection: 100000^3 = 10^15 overflows int32 (~2.1e9) but fits int64
$big = NDArray::fromArray([100000, 100000, 100000], 'int32');
$p = $big->prod();
echo $p === 1000000000000000 ? "prod-overflow-protected-ok\n" : "prod-overflow-FAIL: $p\n";

// bool → int64 (consistent with sum)
$b = NDArray::fromArray([true, true, false], 'bool');
echo $b->prod(0)->dtype() === 'int64' ? "prod-bool-int64-ok\n" : "prod-bool-int64-FAIL\n";
echo $b->prod() === 0 ? "prod-bool-val-ok\n" : "prod-bool-val-FAIL\n";

// f32 stays f32, f64 stays f64
$f32 = NDArray::full([2, 2], 1.5, 'float32');
echo $f32->prod(0)->dtype() === 'float32' ? "prod-f32-ok\n" : "prod-f32-FAIL\n";
$f64 = NDArray::fromArray([1.5, 2.0]);
echo $f64->prod() === 3.0 ? "prod-f64-val-ok\n" : "prod-f64-val-FAIL\n";

/* ===== prod NaN propagation ===== */

$nf = NDArray::fromArray([1.0, 2.0, NAN, 4.0]);
echo is_nan($nf->prod()) ? "prod-nan-prop-ok\n" : "prod-nan-prop-FAIL\n";

/* ===== nanprod ===== */

// NaN treated as multiplicative identity (1)
var_dump($nf->nanprod());                         // float(8) — 1*2*4 = 8

// All-NaN slice → 1 (multiplicative identity), no exception
$an = NDArray::fromArray([NAN, NAN, NAN]);
var_dump($an->nanprod());                         // float(1)

// nanprod on int aliases prod (no NaN possible)
echo $ai->nanprod() === 24 ? "nanprod-int-alias-ok\n" : "nanprod-int-alias-FAIL\n";

/* ===== prod empty-array identity ===== */

$ze = NDArray::zeros([0]);
var_dump($ze->prod());                            // float(1) — multiplicative identity

$zei = NDArray::zeros([0], 'int32');
var_dump($zei->prod());                           // int(1)

/* ===== countNonzero ===== */

$ar = NDArray::fromArray([0, 1, 0, 2, 3, 0, 4]);
var_dump($ar->countNonzero());                    // int(4)

// Output dtype always int64
echo $ar->countNonzero(0)->dtype() === 'int64' ? "countNZ-int-dtype-ok\n" : "countNZ-int-dtype-FAIL\n";

// 2-D axis
$m2 = NDArray::fromArray([[0, 1, 0], [2, 0, 3]]);
var_dump($m2->countNonzero());                    // int(3)
var_dump($m2->countNonzero(0)->toArray());        // [1, 1, 1]
var_dump($m2->countNonzero(1)->toArray());        // [1, 2]

// Bool input
$bv = NDArray::fromArray([true, false, true, true, false], 'bool');
var_dump($bv->countNonzero());                    // int(3)
echo $bv->countNonzero(0)->dtype() === 'int64' ? "countNZ-bool-dtype-ok\n" : "countNZ-bool-dtype-FAIL\n";

// NaN counts as non-zero (matches NumPy: bool(NAN) === true)
$fnz = NDArray::fromArray([0.0, NAN, 1.5, 0.0]);
var_dump($fnz->countNonzero());                   // int(2)

// Empty
var_dump(NDArray::zeros([0])->countNonzero());    // int(0)

/* ===== ptp ===== */

$pa = NDArray::fromArray([1, 5, 3, 9, 2]);
var_dump($pa->ptp());                             // int(8) — 9 - 1

$pm = NDArray::fromArray([[1, 5, 9], [2, 3, 4]]);
var_dump($pm->ptp());                             // int(8)
var_dump($pm->ptp(0)->toArray());                 // [1, 2, 5]
var_dump($pm->ptp(1)->toArray());                 // [8, 2]

// dtype preservation
echo $pa->ptp(0)->dtype() === 'int64' ? "ptp-int64-ok\n" : "ptp-int64-FAIL\n";

$pf = NDArray::fromArray([1.5, 4.0, 2.5]);
echo $pf->ptp() === 2.5 ? "ptp-f64-val-ok\n" : "ptp-f64-val-FAIL\n";

// f32 ptp stays f32
$pf32 = NDArray::full([3], 2.0, 'float32');
echo $pf32->ptp(0)->dtype() === 'float32' ? "ptp-f32-dtype-ok\n" : "ptp-f32-dtype-FAIL\n";

// Bool ptp: true - false === true (matches NumPy)
$pb = NDArray::fromArray([true, false], 'bool');
$bptp = $pb->ptp();
var_dump($bptp);                                  // bool(true)
$pball = NDArray::fromArray([true, true, true], 'bool');
var_dump($pball->ptp());                          // bool(false) — same value

// ptp NaN propagates
echo is_nan(NDArray::fromArray([1.0, NAN, 2.0])->ptp()) ? "ptp-nan-prop-ok\n" : "ptp-nan-prop-FAIL\n";

// ptp empty → throws
try {
    NDArray::zeros([0])->ptp();
    echo "ptp-empty-no-throw-FAIL\n";
} catch (\NDArrayException $e) {
    echo "ptp-empty-throws: ", $e->getMessage(), "\n";
}

// axis OOR
try { $pm->ptp(5); } catch (\ShapeException $e) { echo "ptp-axis-oor: ", $e->getMessage(), "\n"; }
?>
--EXPECT--
int(24)
int(720)
array(3) {
  [0]=>
  int(4)
  [1]=>
  int(10)
  [2]=>
  int(18)
}
array(2) {
  [0]=>
  int(6)
  [1]=>
  int(120)
}
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(3)
}
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(1)
}
array(2) {
  [0]=>
  int(6)
  [1]=>
  int(120)
}
prod-int-ok
prod-int32-promote-ok
prod-int64-stays-ok
prod-overflow-protected-ok
prod-bool-int64-ok
prod-bool-val-ok
prod-f32-ok
prod-f64-val-ok
prod-nan-prop-ok
float(8)
float(1)
nanprod-int-alias-ok
float(1)
int(1)
int(4)
countNZ-int-dtype-ok
int(3)
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(1)
  [2]=>
  int(1)
}
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(2)
}
int(3)
countNZ-bool-dtype-ok
int(2)
int(0)
int(8)
int(8)
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(5)
}
array(2) {
  [0]=>
  int(8)
  [1]=>
  int(2)
}
ptp-int64-ok
ptp-f64-val-ok
ptp-f32-dtype-ok
bool(true)
bool(false)
ptp-nan-prop-ok
ptp-empty-throws: ptp: empty array
ptp-axis-oor: axis 5 out of range for ndim 2
