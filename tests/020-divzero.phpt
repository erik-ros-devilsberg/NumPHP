--TEST--
Division by zero — IEEE for float, DivisionByZeroError for int
--FILE--
<?php
// Float divzero → IEEE
$a = NDArray::fromArray([1.0, -1.0, 0.0]);
$b = NDArray::fromArray([0.0, 0.0, 0.0]);
$r = NDArray::divide($a, $b)->toArray();
var_dump(is_infinite($r[0]) && $r[0] > 0);    // +inf
var_dump(is_infinite($r[1]) && $r[1] < 0);    // -inf
var_dump(is_nan($r[2]));                       // 0/0 → nan

// Int divzero → exception
$x = NDArray::fromArray([1, 2, 3]);
$y = NDArray::fromArray([1, 0, 1]);
try {
    NDArray::divide($x, $y);
    echo "no exception (unexpected)\n";
} catch (DivisionByZeroError $e) {
    echo "int-divzero: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
int-divzero: Division by zero
