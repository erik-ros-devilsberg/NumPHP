--TEST--
fromArray ∘ toArray round-trips; clone produces a deep copy
--FILE--
<?php
$cases = [
    [1, 2, 3, 4],
    [[1, 2], [3, 4], [5, 6]],
    [[[1, 2], [3, 4]], [[5, 6], [7, 8]]],
];
foreach ($cases as $orig) {
    $a = NDArray::fromArray($orig);
    var_dump($a->toArray() === $orig);
}

// clone is a deep copy: mutating the original buffer (via re-creation) cannot affect the clone
$a = NDArray::fromArray([[1, 2], [3, 4]]);
$b = clone $a;
unset($a);
var_dump($b->toArray());
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
array(2) {
  [0]=>
  array(2) {
    [0]=>
    int(1)
    [1]=>
    int(2)
  }
  [1]=>
  array(2) {
    [0]=>
    int(3)
    [1]=>
    int(4)
  }
}
