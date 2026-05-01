--TEST--
NaN-aware cumulative variants — propagation and skip
--FILE--
<?php
// f64: NaN at index 2 → output[2..] all NaN
$a = NDArray::fromArray([1.0, 2.0, NAN, 4.0, 5.0]);
$cs = $a->cumsum()->toArray();
echo $cs[0] === 1.0 && $cs[1] === 3.0 ? "cumsum-pre-ok\n" : "cumsum-pre-FAIL\n";
echo is_nan($cs[2]) && is_nan($cs[3]) && is_nan($cs[4]) ? "cumsum-prop-ok\n" : "cumsum-prop-FAIL\n";

// f32 cumprod: NaN propagates the same way
$f = NDArray::fromArray([2.0, 3.0, NAN, 5.0], 'float32');
$cp = $f->cumprod()->toArray();
echo $cp[0] === 2.0 && $cp[1] === 6.0 ? "f32-cumprod-pre-ok\n" : "f32-cumprod-pre-FAIL\n";
echo is_nan($cp[2]) && is_nan($cp[3]) ? "f32-cumprod-prop-ok\n" : "f32-cumprod-prop-FAIL\n";

// nancumsum — skip NaN (treat as 0)
var_dump($a->nancumsum()->toArray());              // [1, 3, 3, 7, 12]

// nancumprod — skip NaN (treat as 1)
var_dump($a->nancumprod()->toArray());             // [1, 2, 2, 8, 40]

// All-NaN slice — nancumsum → all 0, nancumprod → all 1
$all = NDArray::fromArray([NAN, NAN, NAN]);
var_dump($all->nancumsum()->toArray());            // [0, 0, 0]
var_dump($all->nancumprod()->toArray());           // [1, 1, 1]

// 2-D nancumsum along axis 1
$m = NDArray::fromArray([[1.0, NAN, 3.0], [NAN, 2.0, 4.0]]);
var_dump($m->nancumsum(1)->toArray());             // [[1,1,4],[0,2,6]]
var_dump($m->nancumprod(0)->toArray());            // [[1,1,3],[1,2,12]]

// Integer dtype: nancumsum aliases cumsum, nancumprod aliases cumprod
$ai = NDArray::fromArray([1, 2, 3, 4], 'int32');
var_dump($ai->nancumsum()->dtype());               // int64
var_dump($ai->nancumsum()->toArray());             // [1, 3, 6, 10]
var_dump($ai->nancumprod()->toArray());            // [1, 2, 6, 24]
?>
--EXPECT--
cumsum-pre-ok
cumsum-prop-ok
f32-cumprod-pre-ok
f32-cumprod-prop-ok
array(5) {
  [0]=>
  float(1)
  [1]=>
  float(3)
  [2]=>
  float(3)
  [3]=>
  float(7)
  [4]=>
  float(12)
}
array(5) {
  [0]=>
  float(1)
  [1]=>
  float(2)
  [2]=>
  float(2)
  [3]=>
  float(8)
  [4]=>
  float(40)
}
array(3) {
  [0]=>
  float(0)
  [1]=>
  float(0)
  [2]=>
  float(0)
}
array(3) {
  [0]=>
  float(1)
  [1]=>
  float(1)
  [2]=>
  float(1)
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    float(1)
    [1]=>
    float(1)
    [2]=>
    float(4)
  }
  [1]=>
  array(3) {
    [0]=>
    float(0)
    [1]=>
    float(2)
    [2]=>
    float(6)
  }
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    float(1)
    [1]=>
    float(1)
    [2]=>
    float(3)
  }
  [1]=>
  array(3) {
    [0]=>
    float(1)
    [1]=>
    float(2)
    [2]=>
    float(12)
  }
}
string(5) "int64"
array(4) {
  [0]=>
  int(1)
  [1]=>
  int(3)
  [2]=>
  int(6)
  [3]=>
  int(10)
}
array(4) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(6)
  [3]=>
  int(24)
}
