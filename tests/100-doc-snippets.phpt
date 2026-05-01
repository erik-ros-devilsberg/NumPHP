--TEST--
Doc snippet harness: every ```php block in user-facing docs runs and matches its expected output
--FILE--
<?php
require __DIR__ . '/_helpers/snippet_runner.php';

$repo_root = dirname(__DIR__);
$by_file = snippet_extract($repo_root);

if ($by_file === []) {
    echo "FAIL: no snippets found — extractor or path filter regressed\n";
    exit(1);
}

$total_run = 0;
$total_skipped = 0;
$failures = [];

foreach ($by_file as $path => $snippets) {
    $rel = str_replace($repo_root . '/', '', $path);
    $r = snippet_run_file($snippets);
    $total_run += $r['run'];
    $total_skipped += $r['skipped'];

    if (!$r['ok']) {
        $msg = "$rel: " . $r['error'];
        if (isset($r['expected'])) {
            $msg .= "\n  expected: " . $r['expected'];
            $msg .= "\n  actual:   " . $r['actual'];
        } elseif (isset($r['actual'])) {
            $msg .= "\n  partial output: " . trim($r['actual']);
        }
        $failures[] = $msg;
    }
}

if ($failures !== []) {
    foreach ($failures as $f) echo "FAIL: $f\n";
    echo "summary: " . count($failures) . " files failed, $total_run run, $total_skipped skipped\n";
    exit(1);
}

printf("all-passed (%d files, %d snippets run, %d skipped)\n", count($by_file), $total_run, $total_skipped);
?>
--EXPECTF--
all-passed (%d files, %d snippets run, %d skipped)
