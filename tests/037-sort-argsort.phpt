--TEST--
sort / argsort — axis, flatten, dtype
--FILE--
<?php
// 1D sort (axis defaults to -1 = last; for 1D that's the only axis)
$a = NDArray::fromArray([3, 1, 4, 8, 5, 9, 2, 6]);     // distinct — qsort is unstable so no ties
var_dump($a->sort()->toArray());                       // [1, 2, 3, 4, 5, 6, 8, 9]

// argsort returns int64 indices, with distinct values the answer is unique
$as = $a->argsort();
var_dump($as->dtype());                                // int64
var_dump($as->toArray());                              // [1, 6, 0, 2, 4, 7, 3, 5]

// Verify argsort gives correct ordering by re-gathering
$idx = $as->toArray();
$src = $a->toArray();
$gathered = [];
foreach ($idx as $i) $gathered[] = $src[$i];
var_dump($gathered);                                   // [1, 2, 3, 4, 5, 6, 8, 9]

// 2D — sort along axis 1 (default)
$m = NDArray::fromArray([[3, 1, 4], [9, 5, 2]]);
var_dump($m->sort()->toArray());                       // [[1,3,4], [2,5,9]]
var_dump($m->sort(1)->toArray());                      // same — explicit axis=1
var_dump($m->sort(0)->toArray());                      // [[3,1,2], [9,5,4]]

// argsort axis=0
var_dump($m->argsort(0)->toArray());                   // [[0,0,1], [1,1,0]]
var_dump($m->argsort(0)->dtype());                     // int64

// axis=null flattens, sorts as 1-D
var_dump($m->sort(null)->shape());                     // [6]
var_dump($m->sort(null)->toArray());                   // [1, 2, 3, 4, 5, 9]
var_dump($m->argsort(null)->toArray());                // flat indices

// Negative axis
var_dump($m->sort(-1)->toArray());                     // axis=1 result

// dtype preserved on sort
var_dump(NDArray::fromArray([3.5, 1.5, 2.5])->sort()->dtype());  // float64
$f32 = NDArray::full([3], 1.0, 'float32');
var_dump($f32->sort()->dtype());                       // float32

// Out-of-range axis throws
try { $m->sort(5); } catch (ShapeException $e) { echo "axis-oor: ", $e->getMessage(), "\n"; }
?>
--EXPECT--
array(8) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
  [3]=>
  int(4)
  [4]=>
  int(5)
  [5]=>
  int(6)
  [6]=>
  int(8)
  [7]=>
  int(9)
}
string(5) "int64"
array(8) {
  [0]=>
  int(1)
  [1]=>
  int(6)
  [2]=>
  int(0)
  [3]=>
  int(2)
  [4]=>
  int(4)
  [5]=>
  int(7)
  [6]=>
  int(3)
  [7]=>
  int(5)
}
array(8) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
  [3]=>
  int(4)
  [4]=>
  int(5)
  [5]=>
  int(6)
  [6]=>
  int(8)
  [7]=>
  int(9)
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    int(1)
    [1]=>
    int(3)
    [2]=>
    int(4)
  }
  [1]=>
  array(3) {
    [0]=>
    int(2)
    [1]=>
    int(5)
    [2]=>
    int(9)
  }
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    int(1)
    [1]=>
    int(3)
    [2]=>
    int(4)
  }
  [1]=>
  array(3) {
    [0]=>
    int(2)
    [1]=>
    int(5)
    [2]=>
    int(9)
  }
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    int(3)
    [1]=>
    int(1)
    [2]=>
    int(2)
  }
  [1]=>
  array(3) {
    [0]=>
    int(9)
    [1]=>
    int(5)
    [2]=>
    int(4)
  }
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    int(0)
    [1]=>
    int(0)
    [2]=>
    int(1)
  }
  [1]=>
  array(3) {
    [0]=>
    int(1)
    [1]=>
    int(1)
    [2]=>
    int(0)
  }
}
string(5) "int64"
array(1) {
  [0]=>
  int(6)
}
array(6) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
  [3]=>
  int(4)
  [4]=>
  int(5)
  [5]=>
  int(9)
}
array(6) {
  [0]=>
  int(1)
  [1]=>
  int(5)
  [2]=>
  int(0)
  [3]=>
  int(2)
  [4]=>
  int(4)
  [5]=>
  int(3)
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    int(1)
    [1]=>
    int(3)
    [2]=>
    int(4)
  }
  [1]=>
  array(3) {
    [0]=>
    int(2)
    [1]=>
    int(5)
    [2]=>
    int(9)
  }
}
string(7) "float64"
string(7) "float32"
axis-oor: axis 5 out of range for ndim 2
