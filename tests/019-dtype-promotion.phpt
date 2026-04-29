--TEST--
Mixed-dtype operations promote per the system.md table
--FILE--
<?php
$f32 = NDArray::ones([2], 'float32');
$f64 = NDArray::ones([2], 'float64');
$i32 = NDArray::ones([2], 'int32');
$i64 = NDArray::ones([2], 'int64');

var_dump(NDArray::add($f32, $i32)->dtype());   // f32
var_dump(NDArray::add($f32, $i64)->dtype());   // f64
var_dump(NDArray::add($i32, $i64)->dtype());   // i64
var_dump(NDArray::add($i32, $i32)->dtype());   // i32
var_dump(NDArray::add($f64, $i32)->dtype());   // f64
var_dump(NDArray::add($f32, $f64)->dtype());   // f64

// scalar PHP int → i64 → promotes
var_dump(($i32 + 1)->dtype());                  // i64
var_dump(($i32 + 1.0)->dtype());                // f64
var_dump(($f32 + 1)->dtype());                  // f64
?>
--EXPECT--
string(7) "float32"
string(7) "float64"
string(5) "int64"
string(5) "int32"
string(7) "float64"
string(7) "float64"
string(5) "int64"
string(7) "float64"
string(7) "float64"
