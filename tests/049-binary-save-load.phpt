--TEST--
Binary save/load — round-trip every dtype/shape combination, including non-contig
--FILE--
<?php
$path = tempnam(sys_get_temp_dir(), 'numphp_bin_');

function check_roundtrip(NDArray $a, string $tag, string $path): void {
    $a->save($path);
    $b = NDArray::load($path);
    $eq = ($a->dtype() === $b->dtype())
       && ($a->shape() === $b->shape())
       && (json_encode($a->toArray()) === json_encode($b->toArray()));
    echo $eq ? "$tag-ok\n" : "$tag-FAIL\n";
}

// Each dtype × {1-D, 2-D}
foreach (['float32', 'float64', 'int32', 'int64'] as $dt) {
    check_roundtrip(NDArray::fromArray([1, 2, 3, 4, 5], $dt), "1d-$dt", $path);
    check_roundtrip(NDArray::fromArray([[1, 2], [3, 4]], $dt), "2d-$dt", $path);
}

// 3-D
check_roundtrip(NDArray::fromArray([[[1.0, 2.0], [3.0, 4.0]], [[5.0, 6.0], [7.0, 8.0]]]), "3d", $path);

// 0-D
check_roundtrip(NDArray::full([], 7.5, 'float64'), "0d", $path);

// Empty (size 0)
check_roundtrip(NDArray::zeros([0]), "empty", $path);

// Non-contiguous transpose view — saver must materialize
$big = NDArray::fromArray([[1, 2, 3], [4, 5, 6]]);
$tv = $big->transpose();   // shape (3, 2), F-contig
$tv->save($path);
$back = NDArray::load($path);
echo $back->shape() === [3, 2] ? "transpose-shape-ok\n" : "transpose-shape-FAIL\n";
echo $back->toArray() === [[1, 4], [2, 5], [3, 6]] ? "transpose-vals-ok\n" : "transpose-vals-FAIL\n";

// Magic byte 7 is format version 1
$big->save($path);
$header = file_get_contents($path, false, null, 0, 8);
echo bin2hex($header) === "4e554d5048500001" ? "magic-ok\n" : "magic-FAIL: " . bin2hex($header) . "\n";

unlink($path);
?>
--EXPECT--
1d-float32-ok
2d-float32-ok
1d-float64-ok
2d-float64-ok
1d-int32-ok
2d-int32-ok
1d-int64-ok
2d-int64-ok
3d-ok
0d-ok
empty-ok
transpose-shape-ok
transpose-vals-ok
magic-ok
