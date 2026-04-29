--TEST--
CSV round-trip: write, read back, compare bit-exact for f64
--FILE--
<?php
$path = tempnam(sys_get_temp_dir(), 'numphp_csv_');

// 2-D float64 round-trip — bit-exact via %.17g
$a = NDArray::fromArray([[1.5, 2.25, 3.125], [-4.0, 0.0, 1e-7]]);
$a->toCsv($path);
$b = NDArray::fromCsv($path);
echo $a->toArray() === $b->toArray() ? "f64-2d-bitexact\n" : "f64-2d-FAIL\n";
echo $b->dtype() === 'float64' ? "f64-dtype-ok\n" : "f64-dtype-FAIL\n";
echo $b->shape() === [2, 3] ? "f64-shape-ok\n" : "f64-shape-FAIL\n";

// 1-D writes one cell per row; reader brings it back as 2-D (n, 1)
$v = NDArray::fromArray([10.0, 20.0, 30.0]);
$v->toCsv($path);
$rv = NDArray::fromCsv($path);
echo $rv->shape() === [3, 1] ? "1d-shape-ok\n" : "1d-shape-FAIL: " . json_encode($rv->shape()) . "\n";
echo $rv->toArray() === [[10.0], [20.0], [30.0]] ? "1d-vals-ok\n" : "1d-vals-FAIL\n";

// Header-skip
file_put_contents($path, "a,b,c\n1.0,2.0,3.0\n4.0,5.0,6.0\n");
$with_header = NDArray::fromCsv($path, 'float64', true);
echo $with_header->shape() === [2, 3] ? "header-skip-ok\n" : "header-skip-FAIL\n";

// int dtype
file_put_contents($path, "1,2,3\n4,5,6\n");
$ai = NDArray::fromCsv($path, 'int64');
echo $ai->dtype() === 'int64' ? "int-dtype-ok\n" : "int-dtype-FAIL\n";
echo $ai->toArray() === [[1, 2, 3], [4, 5, 6]] ? "int-vals-ok\n" : "int-vals-FAIL\n";

// f32
file_put_contents($path, "1.5,2.5\n3.5,4.5\n");
$f32 = NDArray::fromCsv($path, 'float32');
echo $f32->dtype() === 'float32' ? "f32-dtype-ok\n" : "f32-dtype-FAIL\n";

// Non-contiguous source: writer must materialize first
$big = NDArray::fromArray([[1.0, 2.0, 3.0], [4.0, 5.0, 6.0]]);
$tv = $big->transpose();              // shape (3, 2), non-C-contig
$tv->toCsv($path);
$back = NDArray::fromCsv($path);
echo $back->toArray() === [[1.0, 4.0], [2.0, 5.0], [3.0, 6.0]] ? "transpose-write-ok\n" : "transpose-write-FAIL\n";

unlink($path);
?>
--EXPECT--
f64-2d-bitexact
f64-dtype-ok
f64-shape-ok
1d-shape-ok
1d-vals-ok
header-skip-ok
int-dtype-ok
int-vals-ok
f32-dtype-ok
transpose-write-ok
