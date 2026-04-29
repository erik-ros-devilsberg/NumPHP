--TEST--
BufferView — non-C-contiguous source throws; copy() then view succeeds
--FILE--
<?php
$a = NDArray::fromArray([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]);

// Transpose: view, F-contiguous (not C-contig)
$t = $a->transpose();
try {
    $t->bufferView();
    echo "FAIL\n";
} catch (NDArrayException $e) {
    echo strpos($e->getMessage(), "C-contiguous") !== false
        ? "transpose-rejected\n"
        : "wrong-msg: " . $e->getMessage() . "\n";
}

// Workaround: clone (deep-copies into a fresh C-contig buffer)
$tc = clone $t;
$bv = $tc->bufferView();
echo "clone-then-view-ok\n";
echo "shape=", json_encode($bv->shape), "\n";
echo "strides=", json_encode($bv->strides), "\n";   // C-contig strides for shape (3, 2)

// reshape on C-contig source returns a view that's still C-contig — works
$r = NDArray::fromArray([1, 2, 3, 4, 5, 6])->reshape([2, 3]);
$bv2 = $r->bufferView();
echo "reshape-c-contig-view-ok\n";

// 0-D scalar — has no strides, but is trivially C-contig; should work
$s = NDArray::full([], 42, 'int64');
$bv3 = $s->bufferView();
echo "0d-shape=", json_encode($bv3->shape), "\n";
echo "0d-strides=", json_encode($bv3->strides), "\n";

// Empty array (size 0): allowed, ptr may be 0 or non-zero (impl-defined)
$e = NDArray::zeros([0]);
$bv4 = $e->bufferView();
echo "empty-shape=", json_encode($bv4->shape), "\n";
?>
--EXPECT--
transpose-rejected
clone-then-view-ok
shape=[3,2]
strides=[16,8]
reshape-c-contig-view-ok
0d-shape=[]
0d-strides=[]
empty-shape=[0]
