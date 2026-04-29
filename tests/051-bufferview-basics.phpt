--TEST--
BufferView — basic properties (dtype, shape, strides, ptr, writeable)
--FILE--
<?php
$a = NDArray::fromArray([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]);
$bv = $a->bufferView();

// Class type + finality
echo get_class($bv), "\n";
echo (new ReflectionClass(BufferView::class))->isFinal() ? "final\n" : "not-final\n";

// Public properties
var_dump($bv->dtype);
var_dump($bv->shape);
var_dump($bv->strides);
var_dump($bv->writeable);

// $ptr is a non-zero int (the address of the underlying buffer)
echo is_int($bv->ptr) ? "ptr-is-int\n" : "ptr-NOT-int\n";
echo $bv->ptr !== 0 ? "ptr-nonzero\n" : "ptr-zero\n";

// Default writeable is false
$bv_default = $a->bufferView();
var_dump($bv_default->writeable);

// Explicit writeable=true
$bv_w = $a->bufferView(true);
var_dump($bv_w->writeable);

// f32 dtype
$f32 = NDArray::full([4], 1.5, 'float32');
$bv_f32 = $f32->bufferView();
var_dump($bv_f32->dtype);
var_dump($bv_f32->strides);   // [4] — itemsize for f32

// Cannot construct directly (private/no constructor)
try {
    $r = new ReflectionClass(BufferView::class);
    $constructor = $r->getConstructor();
    if ($constructor === null) {
        echo "no-public-constructor\n";
    } else {
        echo $constructor->isPublic() ? "FAIL-public-ctor\n" : "no-public-constructor\n";
    }
} catch (Throwable $e) { echo "no-public-constructor\n"; }
?>
--EXPECT--
BufferView
final
string(7) "float64"
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(3)
}
array(2) {
  [0]=>
  int(24)
  [1]=>
  int(8)
}
bool(false)
ptr-is-int
ptr-nonzero
bool(false)
bool(true)
string(7) "float32"
array(1) {
  [0]=>
  int(4)
}
no-public-constructor
