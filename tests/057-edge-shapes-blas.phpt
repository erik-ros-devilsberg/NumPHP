--TEST--
BLAS edge shapes — k=1 in matmul / dot / outer
--FILE--
<?php
// (1, k) × (k, 1) — degenerates to a 1×1 matrix (a · b dot product as outer)
$a = NDArray::fromArray([[1.0, 2.0, 3.0]]);          // shape [1, 3]
$b = NDArray::fromArray([[4.0], [5.0], [6.0]]);      // shape [3, 1]
$m = NDArray::matmul($a, $b);
echo "(1,3)x(3,1) shape: ", json_encode($m->shape()), "\n";   // [1, 1]
echo "(1,3)x(3,1) value: ", $m->toArray()[0][0], "\n";        // 1*4 + 2*5 + 3*6 = 32

// (k,) · (k,) with k=1 — single-element dot
$x = NDArray::fromArray([7.0]);
$y = NDArray::fromArray([8.0]);
echo "1-elem dot: ", NDArray::dot($x, $y), "\n";              // 56

// (n, 1) × (1, m) — outer-like via matmul
$col = NDArray::fromArray([[1.0], [2.0], [3.0]]);   // (3, 1)
$row = NDArray::fromArray([[4.0, 5.0]]);            // (1, 2)
$o = NDArray::matmul($col, $row);
echo "(3,1)x(1,2) shape: ", json_encode($o->shape()), "\n";   // [3, 2]
foreach ($o->toArray() as $r) echo "  ", json_encode($r), "\n";
// rows: [4,5], [8,10], [12,15]

// outer of two 1-D arrays — independent BLAS path
$out = NDArray::outer($x, $y);
echo "outer 1x1 shape: ", json_encode($out->shape()), "\n";   // [1, 1]
echo "outer 1x1 value: ", $out->toArray()[0][0], "\n";        // 56
?>
--EXPECT--
(1,3)x(3,1) shape: [1,1]
(1,3)x(3,1) value: 32
1-elem dot: 56
(3,1)x(1,2) shape: [3,2]
  [4,5]
  [8,10]
  [12,15]
outer 1x1 shape: [1,1]
outer 1x1 value: 56
