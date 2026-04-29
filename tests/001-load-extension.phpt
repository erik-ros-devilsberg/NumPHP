--TEST--
Extension loads successfully
--FILE--
<?php
var_dump(extension_loaded('numphp'));
?>
--EXPECT--
bool(true)
