<?php
/**
 * Treating a 2-D NDArray as a grayscale image.
 *
 * Builds a 16×16 gradient, demonstrates element-wise threshold (compare-then-
 * cast) to produce a binary mask, takes a view of a 4×4 tile that straddles
 * the threshold boundary, and writes the mask as a P5 PGM (portable
 * graymap) for inspection. The PGM is round-tripped — read back, compared
 * cell-for-cell, to prove the I/O is honest.
 *
 * Demonstrates: dtype promotion (int subtract → float), view-vs-copy
 * (slice gives a view that shares the source buffer), and that NDArray
 * buffers are the natural representation for pixel data.
 */

$h = 16; $w = 16;

// Gradient: pixel(i,j) = i*w + j → ranges 0..(h*w-1) across the image.
$flat = NDArray::arange(0, $h * $w, 1, 'int64');
$img = $flat->reshape([$h, $w]);

// Threshold at 127.5 — produce a binary mask. There is no dedicated
// comparison op in v1, so we shift, multiply, clip to [0,1], and round:
// the limit of (clip((x - t) * K, 0, 1)) as K → ∞ is the indicator function.
$threshold = 127.5;
$shifted = NDArray::subtract($img, $threshold);
$scaled  = NDArray::multiply($shifted, 1e6);
$mask    = $scaled->clip(0.0, 1.0)->round();        // 0.0 or 1.0
echo "mask dtype: ", $mask->dtype(), "\n";

echo "full mask (· = 0, # = 1):\n";
foreach ($mask->toArray() as $row) {
    foreach ($row as $v) echo ((int)$v === 1) ? '#' : '.';
    echo "\n";
}

// Take a view of a 4×4 tile that straddles the threshold (rows 6..9, cols 6..9).
// slice() works on axis 0 only, so we transpose between the two slices.
$tile = $mask->slice(6, 10)              // rows 6..9, shape (4, 16)
             ->transpose()->slice(6, 10) // cols 6..9, shape (4, 4) transposed
             ->transpose();              // back to row-major view, shape (4, 4)
echo "tile (rows 6-9, cols 6-9):\n";
foreach ($tile->toArray() as $row) {
    foreach ($row as $v) echo ((int)$v === 1) ? '#' : '.';
    echo "\n";
}

// Write the mask to /tmp as a P5 PGM, maxval 1, then read it back and
// compare to prove the round-trip works.
$path = sys_get_temp_dir() . '/numphp-example-mask.pgm';
$fh = fopen($path, 'wb');
fwrite($fh, "P5\n$w $h\n1\n");
foreach ($mask->toArray() as $row) {
    foreach ($row as $v) fwrite($fh, chr((int)$v));
}
fclose($fh);

$raw = file_get_contents($path);
unlink($path);
$header = "P5\n$w $h\n1\n";
$pixels = substr($raw, strlen($header));
$readback_rows = [];
for ($i = 0; $i < $h; $i++) {
    $readback_rows[] = array_map('ord', str_split(substr($pixels, $i * $w, $w)));
}
$readback = NDArray::fromArray($readback_rows, 'float64');

$diff = NDArray::subtract($mask, $readback);
$max_abs_diff = $diff->abs()->max();        // full reduction → scalar
echo "max |mask - readback| = ", $max_abs_diff, "\n";
echo "wrote ", strlen($raw), " bytes (", $h * $w, " pixels + ", strlen($header), "-byte header)\n";
echo "OK\n";
