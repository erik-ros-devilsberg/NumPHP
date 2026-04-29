--TEST--
Exception class hierarchy is registered correctly
--FILE--
<?php
var_dump(class_exists('NDArrayException'));
var_dump(class_exists('ShapeException'));
var_dump(class_exists('DTypeException'));
var_dump(class_exists('IndexException'));
var_dump(is_subclass_of('NDArrayException', 'RuntimeException'));
var_dump(is_subclass_of('ShapeException', 'NDArrayException'));
var_dump(is_subclass_of('DTypeException', 'NDArrayException'));
var_dump(is_subclass_of('IndexException', 'NDArrayException'));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
