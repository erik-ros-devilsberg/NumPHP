--TEST--
BufferView — refcount keeps the underlying buffer alive after source NDArray is dropped
--FILE--
<?php
function make_view() {
    $a = NDArray::fromArray([1.0, 2.0, 3.0, 4.0]);
    $bv = $a->bufferView();
    // $a goes out of scope here; the BufferView should keep its buffer alive
    return $bv;
}

$bv = make_view();

// Metadata still readable
echo $bv->dtype, "\n";
echo "shape=", json_encode($bv->shape), "\n";
echo "strides=", json_encode($bv->strides), "\n";
echo "ptr-stable=", is_int($bv->ptr) && $bv->ptr !== 0 ? "yes" : "no", "\n";

// Drop the view → buffer should be freed too. We can't directly observe this,
// but absence of segfault/leaks is the test (valgrind catches it in CI).
unset($bv);
echo "view-dropped\n";

// Stress: many views over the same array
$a = NDArray::ones([100]);
$views = [];
for ($i = 0; $i < 50; $i++) $views[] = $a->bufferView();
$first_ptr = $views[0]->ptr;
$last_ptr = $views[49]->ptr;
echo "ptr-shared=", $first_ptr === $last_ptr ? "yes" : "no", "\n";

// Drop source first, then views — should not crash
unset($a);
foreach ($views as $v) unset($v);
echo "stress-ok\n";

// Modifications via the source after view creation are visible through future
// views (not enforced for the captured strides, but the buffer is shared).
$arr = NDArray::fromArray([10.0, 20.0, 30.0]);
$bv1 = $arr->bufferView();
$arr[0] = 99.0;     // mutate source
$bv2 = $arr->bufferView();
echo "same-buffer-ptr=", $bv1->ptr === $bv2->ptr ? "yes" : "no", "\n";
?>
--EXPECT--
float64
shape=[4]
strides=[8]
ptr-stable=yes
view-dropped
ptr-shared=yes
stress-ok
same-buffer-ptr=yes
