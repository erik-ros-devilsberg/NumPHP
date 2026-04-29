--TEST--
Countable interface — count() and ->count()
--FILE--
<?php
$a = NDArray::zeros([3, 4]);
var_dump(count($a));
var_dump($a->count());
var_dump($a instanceof Countable);
var_dump($a instanceof ArrayAccess);

$b = NDArray::zeros([7]);
var_dump(count($b));
?>
--EXPECT--
int(12)
int(12)
bool(true)
bool(true)
int(7)
