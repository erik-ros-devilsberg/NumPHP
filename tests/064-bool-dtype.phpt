--TEST--
bool dtype — factories, fromArray/toArray, save/load, promotion
--FILE--
<?php
// Factories accept 'bool'
var_dump(NDArray::zeros([3], 'bool')->dtype());        // bool
var_dump(NDArray::ones([4], 'bool')->dtype());         // bool
var_dump(NDArray::full([2, 2], 1, 'bool')->dtype());   // bool
var_dump(NDArray::eye(3, null, 0, 'bool')->dtype());   // bool

// zeros/ones values and toArray emits PHP true/false
var_dump(NDArray::zeros([3], 'bool')->toArray());      // [false, false, false]
var_dump(NDArray::ones([3], 'bool')->toArray());       // [true, true, true]

// 1-byte storage exposed via bufferView — itemsize implied by dtype name
$bv = NDArray::eye(3, null, 0, 'bool')->bufferView();
var_dump($bv->dtype);                                  // "bool"

// fromArray with PHP true/false leaves and explicit 'bool'
$a = NDArray::fromArray([true, false, true], 'bool');
var_dump($a->dtype());                                 // bool
var_dump($a->toArray());                               // [true, false, true]

// fromArray with numeric leaves cast under 'bool': 0 → false, anything else → true
var_dump(NDArray::fromArray([1, 0, 5, -3], 'bool')->toArray());
// → [true, false, true, true]
var_dump(NDArray::fromArray([1.5, 0.0, NAN], 'bool')->toArray());
// NaN is non-zero per IEEE — counts as true
// → [true, false, true]

// fromArray default-dtype inference unchanged: PHP true/false → int64 [1, 0]
$d = NDArray::fromArray([true, false]);
var_dump($d->dtype());                                 // int64
var_dump($d->toArray());                               // [1, 0]

// Promotion: bool+bool=bool, bool+int*=int*, bool+float*=float*
$bb = NDArray::add(
    NDArray::fromArray([true, true, false], 'bool'),
    NDArray::fromArray([true, false, false], 'bool'));
var_dump($bb->dtype());                                // bool
var_dump($bb->toArray());                              // [true, true, false] — write-canonicalised
$bi = NDArray::add(
    NDArray::fromArray([true, false], 'bool'),
    NDArray::fromArray([10, 20], 'int32'));
var_dump($bi->dtype());                                // int32
var_dump($bi->toArray());                              // [11, 20]
$bf = NDArray::add(
    NDArray::fromArray([true, false], 'bool'),
    NDArray::fromArray([1.5, 2.5], 'float64'));
var_dump($bf->dtype());                                // float64

// save/load round-trip
$path = tempnam(sys_get_temp_dir(), 'numphp_bool_');
$src = NDArray::fromArray([[true, false, true], [false, true, false]], 'bool');
$src->save($path);
$loaded = NDArray::load($path);
var_dump($loaded->dtype());                            // bool
var_dump($loaded->toArray());
unlink($path);

// Bogus dtype name still rejected
try {
    NDArray::fromArray([[true]], 'bogus');
    echo "FAIL\n";
} catch (DTypeException $e) {
    echo "bogus-dtype-rejected\n";
}
?>
--EXPECT--
string(4) "bool"
string(4) "bool"
string(4) "bool"
string(4) "bool"
array(3) {
  [0]=>
  bool(false)
  [1]=>
  bool(false)
  [2]=>
  bool(false)
}
array(3) {
  [0]=>
  bool(true)
  [1]=>
  bool(true)
  [2]=>
  bool(true)
}
string(4) "bool"
string(4) "bool"
array(3) {
  [0]=>
  bool(true)
  [1]=>
  bool(false)
  [2]=>
  bool(true)
}
array(4) {
  [0]=>
  bool(true)
  [1]=>
  bool(false)
  [2]=>
  bool(true)
  [3]=>
  bool(true)
}
array(3) {
  [0]=>
  bool(true)
  [1]=>
  bool(false)
  [2]=>
  bool(true)
}
string(5) "int64"
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(0)
}
string(4) "bool"
array(3) {
  [0]=>
  bool(true)
  [1]=>
  bool(true)
  [2]=>
  bool(false)
}
string(5) "int32"
array(2) {
  [0]=>
  int(11)
  [1]=>
  int(20)
}
string(7) "float64"
string(4) "bool"
array(2) {
  [0]=>
  array(3) {
    [0]=>
    bool(true)
    [1]=>
    bool(false)
    [2]=>
    bool(true)
  }
  [1]=>
  array(3) {
    [0]=>
    bool(false)
    [1]=>
    bool(true)
    [2]=>
    bool(false)
  }
}
bogus-dtype-rejected
