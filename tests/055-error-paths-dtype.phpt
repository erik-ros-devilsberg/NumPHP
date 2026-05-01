--TEST--
DTypeException paths — every factory rejects an unknown dtype string
--FILE--
<?php
// Every factory takes a dtype name; an unknown name must throw \DTypeException
// (decision 4 in docs/system.md — exception hierarchy).
foreach ([
    fn() => NDArray::zeros([2], "bogus"),
    fn() => NDArray::ones([2], "bogus"),
    fn() => NDArray::full([2], 0, "bogus"),
    fn() => NDArray::eye(2, null, 0, "bogus"),
    fn() => NDArray::arange(0, 4, 1, "bogus"),
    fn() => NDArray::fromArray([1, 2], "bogus"),
] as $i => $f) {
    try { $f(); echo "$i: FAIL no throw\n"; }
    catch (DTypeException $e) { echo "$i: ok (", $e->getMessage(), ")\n"; }
}

// DTypeException is a subclass of NDArrayException — catching the parent
// works too. Lock that hierarchy.
try { NDArray::zeros([2], "bogus"); }
catch (NDArrayException $e) { echo "parent-catch: ok\n"; }
?>
--EXPECT--
0: ok (Unsupported dtype: bogus)
1: ok (Unsupported dtype: bogus)
2: ok (Unsupported dtype: bogus)
3: ok (Unsupported dtype: bogus)
4: ok (Unsupported dtype: bogus)
5: ok (Unsupported dtype: bogus)
parent-catch: ok
