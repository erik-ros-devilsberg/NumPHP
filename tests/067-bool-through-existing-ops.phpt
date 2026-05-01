--TEST--
bool dtype through existing reductions / shape ops / BLAS
--FILE--
<?php
$b = NDArray::fromArray([true, false, true, true, false], 'bool');

// sum on bool → int64 count of trues (decision 34)
var_dump($b->sum());                                    // int(3)
$s = $b->sum(0);   // explicit axis to force NDArray return
var_dump($s->dtype());                                  // int64

// cumsum on bool → int64 (decision 34)
var_dump($b->cumsum()->dtype());                        // int64
var_dump($b->cumsum()->toArray());                      // [1, 1, 2, 3, 3]

// cumprod on bool → int64 (also decision 34, mirrors decision 31)
var_dump($b->cumprod()->dtype());                       // int64
var_dump($b->cumprod()->toArray());                     // [1, 0, 0, 0, 0]

// mean on bool → float64
var_dump($b->mean());                                   // float(0.6)
var_dump($b->mean(0)->dtype());                         // float64

// min / max on bool preserve bool
var_dump($b->min(0)->dtype());                          // bool
var_dump($b->min());                                    // bool(false)
var_dump($b->max());                                    // bool(true)

// argmin / argmax → int64
var_dump($b->argmin());                                 // int (first false: index 1)
var_dump($b->argmax());                                 // int (first true: index 0)

// shape ops on bool — view-based, dtype preserved
$m = NDArray::fromArray([[true, false, true], [false, true, false]], 'bool');
$t = $m->transpose();
var_dump($t->shape());                                  // [3, 2]
var_dump($t->dtype());                                  // bool
var_dump($t->toArray());

// reshape / flatten preserve bool
var_dump($m->flatten()->dtype());                       // bool
var_dump($m->reshape([3, 2])->dtype());                 // bool

// concatenate on bool + bool → bool
$c = NDArray::concatenate([
    NDArray::fromArray([true, false], 'bool'),
    NDArray::fromArray([true, true], 'bool')]);
var_dump($c->dtype());                                  // bool
var_dump($c->toArray());                                // [t, f, t, t]

// concatenate on bool + int32 → int32 (existing concat promotion + new bool entry)
$cm = NDArray::concatenate([
    NDArray::fromArray([true, false], 'bool'),
    NDArray::fromArray([10, 20], 'int32')]);
var_dump($cm->dtype());                                 // int32
var_dump($cm->toArray());                               // [1, 0, 10, 20]
?>
--EXPECT--
int(3)
string(5) "int64"
string(5) "int64"
array(5) {
  [0]=>
  int(1)
  [1]=>
  int(1)
  [2]=>
  int(2)
  [3]=>
  int(3)
  [4]=>
  int(3)
}
string(5) "int64"
array(5) {
  [0]=>
  int(1)
  [1]=>
  int(0)
  [2]=>
  int(0)
  [3]=>
  int(0)
  [4]=>
  int(0)
}
float(0.6)
string(7) "float64"
string(4) "bool"
bool(false)
bool(true)
int(1)
int(0)
array(2) {
  [0]=>
  int(3)
  [1]=>
  int(2)
}
string(4) "bool"
array(3) {
  [0]=>
  array(2) {
    [0]=>
    bool(true)
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
  [2]=>
  array(2) {
    [0]=>
    bool(true)
    [1]=>
    bool(false)
  }
}
string(4) "bool"
string(4) "bool"
string(4) "bool"
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
string(5) "int32"
array(4) {
  [0]=>
  int(1)
  [1]=>
  int(0)
  [2]=>
  int(10)
  [3]=>
  int(20)
}
