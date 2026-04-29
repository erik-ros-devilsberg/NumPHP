--TEST--
Incompatible broadcast shapes throw ShapeException
--FILE--
<?php
$a = NDArray::fromArray([[1, 2, 3], [4, 5, 6]]);   // [2, 3]
$b = NDArray::fromArray([[1, 2], [3, 4]]);          // [2, 2]
try {
    NDArray::add($a, $b);
    echo "no exception\n";
} catch (ShapeException $e) {
    echo "mismatch-1: ", $e->getMessage(), "\n";
}

$c = NDArray::fromArray([1, 2, 3, 4]);              // [4]
$d = NDArray::fromArray([1, 2, 3]);                 // [3]
try {
    NDArray::add($c, $d);
    echo "no exception\n";
} catch (ShapeException $e) {
    echo "mismatch-2: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
mismatch-1: Shape mismatch for broadcast at axis 1 (3 vs 2)
mismatch-2: Shape mismatch for broadcast at axis 0 (4 vs 3)
