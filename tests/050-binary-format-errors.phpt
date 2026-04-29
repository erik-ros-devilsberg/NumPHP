--TEST--
Binary format error cases: bad magic, truncation, unknown dtype/version
--FILE--
<?php
$path = tempnam(sys_get_temp_dir(), 'numphp_bin_err_');

// Bad magic
file_put_contents($path, "BOGUS\0\1" . str_repeat("\0", 100));
try { NDArray::load($path); echo "FAIL\n"; }
catch (NDArrayException $e) { echo "bad-magic\n"; }

// Unknown format version (\xFF instead of \x01)
file_put_contents($path, "NUMPHP\0\xFF" . str_repeat("\0", 100));
try { NDArray::load($path); echo "FAIL\n"; }
catch (NDArrayException $e) {
    echo strpos($e->getMessage(), "version") !== false ? "version-mentioned\n" : "no-version\n";
}

// Truncated file (only the 16-byte header, no body)
$a = NDArray::fromArray([1.0, 2.0, 3.0, 4.0]);
$a->save($path);
$h = file_get_contents($path, false, null, 0, 16 + 8 * 1);  // header + 8 B of body, missing 24 B
file_put_contents($path, $h);
try { NDArray::load($path); echo "FAIL\n"; }
catch (NDArrayException $e) { echo "truncated\n"; }

// Unknown dtype byte (\xFF)
$a->save($path);
$buf = file_get_contents($path);
$buf[8] = "\xFF";   // dtype byte at offset 8
file_put_contents($path, $buf);
try { NDArray::load($path); echo "FAIL\n"; }
catch (DTypeException $e) { echo "bad-dtype\n"; }

// Empty file
file_put_contents($path, "");
try { NDArray::load($path); echo "FAIL\n"; }
catch (NDArrayException $e) { echo "empty\n"; }

unlink($path);
?>
--EXPECT--
bad-magic
version-mentioned
truncated
bad-dtype
empty
