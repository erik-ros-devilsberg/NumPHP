--TEST--
cumsum / cumprod — axis, negative axis, dtype rules
--FILE--
<?php
// 1-D f64 cumsum: axis=null (flatten) and axis=0 should agree
$a = NDArray::fromArray([1.0, 2.0, 3.0, 4.0]);
var_dump($a->cumsum()->toArray());                 // [1, 3, 6, 10]
var_dump($a->cumsum(0)->toArray());                // [1, 3, 6, 10]
var_dump($a->cumprod()->toArray());                // [1, 2, 6, 24]
var_dump($a->cumprod(0)->toArray());               // [1, 2, 6, 24]

// 2-D f64 — shape preserved, axis matters
$m = NDArray::fromArray([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]);
var_dump($m->cumsum(0)->toArray());                // [[1,2,3],[5,7,9]]
var_dump($m->cumsum(1)->toArray());                // [[1,3,6],[4,9,15]]
var_dump($m->cumprod(0)->toArray());               // [[1,2,3],[4,10,18]]
var_dump($m->cumprod(1)->toArray());               // [[1,2,6],[4,20,120]]

// Flatten 2-D with axis=null — output is 1-D of size 6
$flat = $m->cumsum();
var_dump($flat->shape());                          // [6]
var_dump($flat->toArray());                        // [1,3,6,10,15,21]

// Negative axis on 2-D
var_dump($m->cumsum(-1)->toArray());               // [[1,3,6],[4,9,15]]
var_dump($m->cumsum(-2)->toArray());               // [[1,2,3],[5,7,9]]

// Out-of-range axis throws ShapeException
try { $m->cumsum(2); } catch (ShapeException $e) { echo "axis-oor-2: ", $e->getMessage(), "\n"; }
try { $m->cumprod(-3); } catch (ShapeException $e) { echo "axis-oor--3: ", $e->getMessage(), "\n"; }

// f32 input → f32 output, dtype preserved
$f32 = NDArray::full([4], 1.5, 'float32');
var_dump($f32->cumsum()->dtype());                 // float32
var_dump($f32->cumsum()->toArray());               // [1.5, 3.0, 4.5, 6.0]
var_dump($f32->cumprod()->dtype());                // float32

// int32 → int64 (decision 31)
$i32 = NDArray::fromArray([[1, 2, 3], [4, 5, 6]], 'int32');
var_dump($i32->cumsum(0)->dtype());                // int64
var_dump($i32->cumsum(0)->toArray());              // [[1,2,3],[5,7,9]]
var_dump($i32->cumprod(1)->dtype());               // int64
var_dump($i32->cumprod(1)->toArray());             // [[1,2,6],[4,20,120]]

// int64 → int64
$i64 = NDArray::fromArray([1, 2, 3, 4], 'int64');
var_dump($i64->cumsum()->dtype());                 // int64
var_dump($i64->cumsum()->toArray());               // [1, 3, 6, 10]

// size-0 input — empty output, correct shape
$z = NDArray::zeros([0]);
var_dump($z->cumsum()->shape());                   // [0]
var_dump($z->cumsum()->toArray());                 // []
$z2 = NDArray::zeros([0, 3]);
var_dump($z2->cumsum(1)->shape());                 // [0, 3]
var_dump($z2->cumsum(0)->shape());                 // [0, 3]

// 3-D cumsum axis=1 — locks the strided outer-walk path
$c = NDArray::fromArray([
    [[1.0, 2.0], [3.0, 4.0], [5.0, 6.0]],
    [[7.0, 8.0], [9.0, 10.0], [11.0, 12.0]],
]); // shape [2, 3, 2]
var_dump($c->cumsum(1)->shape());                  // [2, 3, 2]
var_dump($c->cumsum(1)->toArray());
// expected:
//   batch 0: [[1,2],[4,6],[9,12]]
//   batch 1: [[7,8],[16,18],[27,30]]
?>
--EXPECT--
array(4) {
  [0]=>
  float(1)
  [1]=>
  float(3)
  [2]=>
  float(6)
  [3]=>
  float(10)
}
array(4) {
  [0]=>
  float(1)
  [1]=>
  float(3)
  [2]=>
  float(6)
  [3]=>
  float(10)
}
array(4) {
  [0]=>
  float(1)
  [1]=>
  float(2)
  [2]=>
  float(6)
  [3]=>
  float(24)
}
array(4) {
  [0]=>
  float(1)
  [1]=>
  float(2)
  [2]=>
  float(6)
  [3]=>
  float(24)
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    float(1)
    [1]=>
    float(2)
    [2]=>
    float(3)
  }
  [1]=>
  array(3) {
    [0]=>
    float(5)
    [1]=>
    float(7)
    [2]=>
    float(9)
  }
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    float(1)
    [1]=>
    float(3)
    [2]=>
    float(6)
  }
  [1]=>
  array(3) {
    [0]=>
    float(4)
    [1]=>
    float(9)
    [2]=>
    float(15)
  }
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    float(1)
    [1]=>
    float(2)
    [2]=>
    float(3)
  }
  [1]=>
  array(3) {
    [0]=>
    float(4)
    [1]=>
    float(10)
    [2]=>
    float(18)
  }
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    float(1)
    [1]=>
    float(2)
    [2]=>
    float(6)
  }
  [1]=>
  array(3) {
    [0]=>
    float(4)
    [1]=>
    float(20)
    [2]=>
    float(120)
  }
}
array(1) {
  [0]=>
  int(6)
}
array(6) {
  [0]=>
  float(1)
  [1]=>
  float(3)
  [2]=>
  float(6)
  [3]=>
  float(10)
  [4]=>
  float(15)
  [5]=>
  float(21)
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    float(1)
    [1]=>
    float(3)
    [2]=>
    float(6)
  }
  [1]=>
  array(3) {
    [0]=>
    float(4)
    [1]=>
    float(9)
    [2]=>
    float(15)
  }
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    float(1)
    [1]=>
    float(2)
    [2]=>
    float(3)
  }
  [1]=>
  array(3) {
    [0]=>
    float(5)
    [1]=>
    float(7)
    [2]=>
    float(9)
  }
}
axis-oor-2: axis 2 out of range for ndim 2
axis-oor--3: axis -3 out of range for ndim 2
string(7) "float32"
array(4) {
  [0]=>
  float(1.5)
  [1]=>
  float(3)
  [2]=>
  float(4.5)
  [3]=>
  float(6)
}
string(7) "float32"
string(5) "int64"
array(2) {
  [0]=>
  array(3) {
    [0]=>
    int(1)
    [1]=>
    int(2)
    [2]=>
    int(3)
  }
  [1]=>
  array(3) {
    [0]=>
    int(5)
    [1]=>
    int(7)
    [2]=>
    int(9)
  }
}
string(5) "int64"
array(2) {
  [0]=>
  array(3) {
    [0]=>
    int(1)
    [1]=>
    int(2)
    [2]=>
    int(6)
  }
  [1]=>
  array(3) {
    [0]=>
    int(4)
    [1]=>
    int(20)
    [2]=>
    int(120)
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
array(1) {
  [0]=>
  int(0)
}
array(0) {
}
array(2) {
  [0]=>
  int(0)
  [1]=>
  int(3)
}
array(2) {
  [0]=>
  int(0)
  [1]=>
  int(3)
}
array(3) {
  [0]=>
  int(2)
  [1]=>
  int(3)
  [2]=>
  int(2)
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    array(2) {
      [0]=>
      float(1)
      [1]=>
      float(2)
    }
    [1]=>
    array(2) {
      [0]=>
      float(4)
      [1]=>
      float(6)
    }
    [2]=>
    array(2) {
      [0]=>
      float(9)
      [1]=>
      float(12)
    }
  }
  [1]=>
  array(3) {
    [0]=>
    array(2) {
      [0]=>
      float(7)
      [1]=>
      float(8)
    }
    [1]=>
    array(2) {
      [0]=>
      float(16)
      [1]=>
      float(18)
    }
    [2]=>
    array(2) {
      [0]=>
      float(27)
      [1]=>
      float(30)
    }
  }
}
