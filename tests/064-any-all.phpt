--TEST--
any / all — short-circuit OR / AND over bool and coerced numeric input
--FILE--
<?php
/* ===== bool input ===== */

// 1-D bool, global
$t = NDArray::fromArray([true, false, true], 'bool');
var_dump($t->any());                              // bool(true)
var_dump($t->all());                              // bool(false)

$f = NDArray::fromArray([false, false, false], 'bool');
var_dump($f->any());                              // bool(false)
var_dump($f->all());                              // bool(false) — empty all-false → any false, all also false

$tt = NDArray::fromArray([true, true, true], 'bool');
var_dump($tt->all());                             // bool(true)
var_dump($tt->any());                             // bool(true)

/* ===== output dtype is bool, even for non-bool input ===== */

$ai = NDArray::fromArray([0, 1, 2], 'int32');
var_dump($ai->any());                             // bool(true)
var_dump($ai->all());                             // bool(false) — has a zero
echo $ai->any(0)->dtype() === 'bool' ? "any-int-dtype-ok\n" : "any-int-dtype-FAIL\n";
echo $ai->all(0)->dtype() === 'bool' ? "all-int-dtype-ok\n" : "all-int-dtype-FAIL\n";

/* ===== float input — NaN counts as true (matches NumPy / (bool)NAN) ===== */

$nf = NDArray::fromArray([0.0, NAN]);
var_dump($nf->any());                             // bool(true)  — NaN is non-zero
var_dump($nf->all());                             // bool(false) — has a zero

$nn = NDArray::fromArray([NAN, NAN]);
var_dump($nn->any());                             // bool(true)
var_dump($nn->all());                             // bool(true)

/* ===== empty-input identities ===== */

$e = NDArray::zeros([0], 'bool');
var_dump($e->any());                              // bool(false)
var_dump($e->all());                              // bool(true) — vacuous truth

/* ===== axis reductions on 2-D bool ===== */

$m = NDArray::fromArray([
    [true,  false, true ],
    [false, false, true ],
], 'bool');

// axis 0: per-column OR / AND
var_dump($m->any(0)->toArray());                  // [true, false, true]
var_dump($m->all(0)->toArray());                  // [false, false, true]

// axis 1: per-row OR / AND
var_dump($m->any(1)->toArray());                  // [true, true]
var_dump($m->all(1)->toArray());                  // [false, false]

// negative axis
var_dump($m->any(-1)->toArray());                 // [true, true]
var_dump($m->all(-2)->toArray());                 // [false, false, true]

// keepdims
var_dump($m->any(0, true)->shape());              // [1, 3]
var_dump($m->all(1, true)->shape());              // [2, 1]
var_dump($m->any(null, true)->shape());           // [1, 1]

/* ===== 3-D bool ===== */

$cube = NDArray::fromArray([
    [[true, false], [false, false]],
    [[false, true], [true, true]],
], 'bool');
var_dump($cube->any());                           // bool(true)
var_dump($cube->all());                           // bool(false)
var_dump($cube->any(0)->toArray());               // [[true, true],[true, true]]
var_dump($cube->all(2)->toArray());               // [[false, false],[false, true]]

/* ===== axis OOR ===== */

try { $m->any(2); }   catch (ShapeException $e) { echo "any-axis-oor: ", $e->getMessage(), "\n"; }
try { $m->all(-3); }  catch (ShapeException $e) { echo "all-axis-oor: ", $e->getMessage(), "\n"; }
?>
--EXPECT--
bool(true)
bool(false)
bool(false)
bool(false)
bool(true)
bool(true)
bool(true)
bool(false)
any-int-dtype-ok
all-int-dtype-ok
bool(true)
bool(false)
bool(true)
bool(true)
bool(false)
bool(true)
array(3) {
  [0]=>
  bool(true)
  [1]=>
  bool(false)
  [2]=>
  bool(true)
}
array(3) {
  [0]=>
  bool(false)
  [1]=>
  bool(false)
  [2]=>
  bool(true)
}
array(2) {
  [0]=>
  bool(true)
  [1]=>
  bool(true)
}
array(2) {
  [0]=>
  bool(false)
  [1]=>
  bool(false)
}
array(2) {
  [0]=>
  bool(true)
  [1]=>
  bool(true)
}
array(3) {
  [0]=>
  bool(false)
  [1]=>
  bool(false)
  [2]=>
  bool(true)
}
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(3)
}
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(1)
}
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(1)
}
bool(true)
bool(false)
array(2) {
  [0]=>
  array(2) {
    [0]=>
    bool(true)
    [1]=>
    bool(true)
  }
  [1]=>
  array(2) {
    [0]=>
    bool(true)
    [1]=>
    bool(true)
  }
}
array(2) {
  [0]=>
  array(2) {
    [0]=>
    bool(false)
    [1]=>
    bool(false)
  }
  [1]=>
  array(2) {
    [0]=>
    bool(false)
    [1]=>
    bool(true)
  }
}
any-axis-oor: axis 2 out of range for ndim 2
all-axis-oor: axis -3 out of range for ndim 2
