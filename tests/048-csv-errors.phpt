--TEST--
CSV error cases + locale-safety lock
--FILE--
<?php
$path = tempnam(sys_get_temp_dir(), 'numphp_csv_err_');

// Ragged rows
file_put_contents($path, "1,2,3\n4,5\n");
try { NDArray::fromCsv($path); echo "FAIL\n"; }
catch (ShapeException $e) { echo "ragged: ", strpos($e->getMessage(), "row") !== false ? "row-mentioned" : "no-row", "\n"; }

// Non-numeric cell
file_put_contents($path, "1.0,2.0,xyz\n3.0,4.0,5.0\n");
try { NDArray::fromCsv($path); echo "FAIL\n"; }
catch (NDArrayException $e) {
    $msg = $e->getMessage();
    echo (strpos($msg, "line") !== false || strpos($msg, "row") !== false) ? "parse-err-located\n" : "parse-err-no-loc\n";
}

// Empty file
file_put_contents($path, "");
try { NDArray::fromCsv($path); echo "FAIL\n"; }
catch (NDArrayException $e) { echo "empty-file\n"; }

// 3-D input to toCsv
$a3 = NDArray::fromArray([[[1, 2], [3, 4]], [[5, 6], [7, 8]]]);
try { $a3->toCsv($path); echo "FAIL\n"; }
catch (ShapeException $e) { echo "3d-rejected\n"; }

// Missing file
try { NDArray::fromCsv('/nonexistent/path/should/not/exist.csv'); echo "FAIL\n"; }
catch (NDArrayException $e) { echo "missing-file\n"; }

// LOCALE LOCK — flip LC_NUMERIC and verify the round-trip stays bit-exact.
// Try a few common European locales; if none are installed we fall through
// silently (best-effort), but if any is, write/read MUST be locale-safe.
$prev = setlocale(LC_NUMERIC, '0');  // save current
$tried = false;
foreach (['de_DE.UTF-8', 'de_DE.utf8', 'fr_FR.UTF-8', 'fr_FR.utf8'] as $loc) {
    if (setlocale(LC_NUMERIC, $loc) !== false) {
        $tried = true;
        $a = NDArray::fromArray([1.5, 2.25]);
        $a->toCsv($path);
        $b = NDArray::fromCsv($path);
        $bv = $b->toArray();
        echo (abs($bv[0][0] - 1.5) < 1e-15 && abs($bv[1][0] - 2.25) < 1e-15)
            ? "locale-safe ($loc)\n" : "locale-FAIL ($loc) got " . json_encode($bv) . "\n";
        break;
    }
}
if (!$tried) echo "locale-skip (no european locale installed)\n";
setlocale(LC_NUMERIC, $prev);

unlink($path);
?>
--EXPECTREGEX--
ragged: row-mentioned
parse-err-located
empty-file
3d-rejected
missing-file
(locale-safe \([^)]+\)|locale-skip \(no european locale installed\))
