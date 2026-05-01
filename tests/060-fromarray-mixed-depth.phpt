--TEST--
fromArray rejects mixed-depth siblings (regression — used to segfault)
--FILE--
<?php
// Used to segfault before the fromarray_walk rank_locked check: when a
// scalar appeared at depth d before a sibling array at the same depth,
// the rank-inference walk would extend ndim past the scalar's leaf depth
// without re-validating, and the subsequent fill walk would dereference
// the scalar as a HashTable.
foreach ([
    [[1, [99]]],         // [[scalar, array]]  — leaf-then-branch at same depth
    [1, [99]],           // [scalar, array]    — same at outer level
    [[[1], 2]],          // [[array, scalar]]  — branch-then-leaf
    [[1, 2], [3]],       // jagged outer: 2-element row then 1-element row
    [[[1, 2], [3]]],     // jagged inner
] as $i => $arg) {
    try {
        NDArray::fromArray($arg);
        echo "$i: FAIL — should have thrown\n";
    } catch (ShapeException $e) {
        echo "$i: ", $e->getMessage(), "\n";
    }
}

// Sanity: well-formed arrays still succeed.
$ok = NDArray::fromArray([[1, 2], [3, 4]]);
echo "well-formed: shape ", json_encode($ok->shape()), "\n";
?>
--EXPECT--
0: Ragged array: array at leaf depth
1: Ragged array: array at leaf depth
2: Ragged array: scalar at non-leaf depth
3: Ragged array: inconsistent dimension lengths
4: Ragged array: inconsistent dimension lengths
well-formed: shape [2,2]
