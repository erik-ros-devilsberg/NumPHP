--TEST--
NaN-aware reduction variants
--FILE--
<?php
$a = NDArray::fromArray([1.0, NAN, 3.0, 4.0]);

// Default reductions propagate NaN
echo is_nan($a->sum())  ? "sum-nan\n"  : "sum-FAIL\n";
echo is_nan($a->mean()) ? "mean-nan\n" : "mean-FAIL\n";

// nan-variants skip NaN
var_dump($a->nansum());                          // 8.0
var_dump($a->nanmean());                         // 8/3
var_dump($a->nanmin());                          // 1.0
var_dump($a->nanmax());                          // 4.0

// nan-arg variants — first non-NaN extreme by flat index
var_dump($a->nanargmin());                       // 0
var_dump($a->nanargmax());                       // 3

// nanvar / nanstd — Welford skipping NaN. Values [1, 3, 4]: pop var = 14/9
$nv = $a->nanvar();
echo abs($nv - 14.0/9.0) < 1e-12 ? "nanvar-ok\n" : "nanvar-fail: $nv\n";

// All-NaN slice — match NumPy:
//   nansum → 0.0 (NaNs treated as zero, additive identity)
//   nanmean / nanmin / nanmax → NaN (no data)
//   nanargmin / nanargmax → throw (no valid index exists)
$all = NDArray::fromArray([NAN, NAN, NAN]);
var_dump($all->nansum());                                  // float(0)
echo is_nan($all->nanmean()) ? "all-nan-mean\n" : "FAIL\n";
echo is_nan($all->nanmin())  ? "all-nan-min\n"  : "FAIL\n";
echo is_nan($all->nanmax())  ? "all-nan-max\n"  : "FAIL\n";

try { $all->nanargmin(); echo "FAIL\n"; }
catch (NDArrayException $e) { echo "all-nan-argmin: ", $e->getMessage(), "\n"; }

// On integer dtype, nan-variants alias the regular form
$ai = NDArray::fromArray([1, 5, 3]);
var_dump($ai->nansum());                         // int(9)
var_dump($ai->nanmin());                         // int(1)
var_dump($ai->nanargmax());                      // int(1)

// Axis: nanmean along axis 0
$m = NDArray::fromArray([[1.0, NAN], [3.0, 4.0], [5.0, 6.0]]);
var_dump($m->nanmean(0)->toArray());             // [3.0, 5.0]
var_dump($m->nansum(1)->toArray());              // [1.0, 7.0, 11.0]
?>
--EXPECT--
sum-nan
mean-nan
float(8)
float(2.6666666666666665)
float(1)
float(4)
int(0)
int(3)
nanvar-ok
float(0)
all-nan-mean
all-nan-min
all-nan-max
all-nan-argmin: nanargmin: all-NaN slice
int(9)
int(1)
int(1)
array(2) {
  [0]=>
  float(3)
  [1]=>
  float(5)
}
array(3) {
  [0]=>
  float(1)
  [1]=>
  float(7)
  [2]=>
  float(11)
}
