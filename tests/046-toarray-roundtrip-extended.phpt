--TEST--
toArray ↔ fromArray round-trip across dtype/shape combinations + non-contig
--FILE--
<?php
function deep_equal(array $a, array $b): bool {
    return json_encode($a) === json_encode($b);
}

$dtypes = ['float32', 'float64', 'int32', 'int64'];

// 1-D
foreach ($dtypes as $dt) {
    $a = NDArray::fromArray([1, 2, 3, 4, 5], $dt);
    $r = $a->toArray();
    echo deep_equal($r, [1, 2, 3, 4, 5]) ? "1d-$dt-ok\n" : "1d-$dt-FAIL\n";
}

// 2-D
foreach ($dtypes as $dt) {
    $a = NDArray::fromArray([[1, 2], [3, 4]], $dt);
    echo deep_equal($a->toArray(), [[1, 2], [3, 4]]) ? "2d-$dt-ok\n" : "2d-$dt-FAIL\n";
}

// 3-D (f64 only — fromArray dtype dispatch is fine, just verify nesting)
$a3 = NDArray::fromArray([[[1.0, 2.0], [3.0, 4.0]], [[5.0, 6.0], [7.0, 8.0]]]);
echo deep_equal($a3->toArray(), [[[1.0, 2.0], [3.0, 4.0]], [[5.0, 6.0], [7.0, 8.0]]]) ? "3d-ok\n" : "3d-FAIL\n";

// Non-contiguous: transpose view round-trips correctly
$m = NDArray::fromArray([[1, 2, 3], [4, 5, 6]]);
$t = $m->transpose();      // view, non-C-contig
echo deep_equal($t->toArray(), [[1, 4], [2, 5], [3, 6]]) ? "transpose-ok\n" : "transpose-FAIL\n";

// 0-D scalar
$s = NDArray::full([], 42, 'int64');
var_dump($s->toArray());                            // int(42)
?>
--EXPECT--
1d-float32-ok
1d-float64-ok
1d-int32-ok
1d-int64-ok
2d-float32-ok
2d-float64-ok
2d-int32-ok
2d-int64-ok
3d-ok
transpose-ok
int(42)
