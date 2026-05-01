--TEST--
IndexException paths — axis-out-of-range across squeeze/expandDims/concatenate/stack
--FILE--
<?php
$a = NDArray::fromArray([[1, 2, 3]]);

// squeeze: axis OOR (positive)
try { $a->squeeze(99); echo "squeeze: FAIL\n"; }
catch (IndexException $e) { echo "squeeze: ", $e->getMessage(), "\n"; }

// squeeze: axis OOR (very-negative — axis -99 normalises to ndim-99)
try { $a->squeeze(-99); echo "squeeze-neg-oor: FAIL\n"; }
catch (IndexException $e) { echo "squeeze-neg-oor: ", $e->getMessage(), "\n"; }

// expandDims: axis OOR (allowed range is [0..ndim], not [0..ndim-1])
try { $a->expandDims(99); echo "expand: FAIL\n"; }
catch (IndexException $e) { echo "expand: ", $e->getMessage(), "\n"; }

// concatenate: axis OOR
$b = NDArray::fromArray([[4, 5, 6]]);
try { NDArray::concatenate([$a, $b], 99); echo "concat-axis: FAIL\n"; }
catch (IndexException $e) { echo "concat-axis: ", $e->getMessage(), "\n"; }

// stack: axis OOR (allowed range is [0..ndim], not [0..ndim-1])
try { NDArray::stack([$a, $b], 99); echo "stack-axis: FAIL\n"; }
catch (IndexException $e) { echo "stack-axis: ", $e->getMessage(), "\n"; }

// concatenate: 0-D arrays cannot be concatenated → ShapeException, not IndexException
$z1 = NDArray::full([], 1);
$z2 = NDArray::full([], 2);
try { NDArray::concatenate([$z1, $z2], 0); echo "concat-0d: FAIL\n"; }
catch (ShapeException $e) { echo "concat-0d: ", $e->getMessage(), "\n"; }
?>
--EXPECT--
squeeze: squeeze: axis out of range
squeeze-neg-oor: squeeze: axis out of range
expand: expandDims: axis out of range
concat-axis: concatenate: axis out of range
stack-axis: stack: axis out of range
concat-0d: concatenate: 0-D arrays cannot be concatenated
