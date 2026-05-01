--TEST--
bench/run.php --tiny smoke — runner doesn't crash, emits one JSON record per scenario
--SKIPIF--
<?php
$so = dirname(__DIR__) . '/modules/numphp.so';
if (!file_exists($so)) {
    echo "skip $so not built";
}
?>
--FILE--
<?php
$root = dirname(__DIR__);
$cmd = sprintf(
    'php -d extension=%s %s --tiny',
    escapeshellarg($root . '/modules/numphp.so'),
    escapeshellarg($root . '/bench/run.php')
);

$descriptors = [
    1 => ['pipe', 'w'],
    2 => ['pipe', 'w'],
];
$proc = proc_open($cmd, $descriptors, $pipes, $root);
$stdout = stream_get_contents($pipes[1]); fclose($pipes[1]);
$stderr = stream_get_contents($pipes[2]); fclose($pipes[2]);
$exit = proc_close($proc);

echo "exit: $exit\n";

$lines = array_values(array_filter(explode("\n", $stdout)));
echo "lines: ", count($lines), "\n";

foreach ($lines as $line) {
    $rec = json_decode($line, true);
    echo "id-prefix: ", substr($rec['id'] ?? 'null', 0, 16), "\n";
    echo "engine: ", $rec['engine'] ?? 'null', "\n";
    echo "median-positive: ", (($rec['median_ns'] ?? 0) > 0 ? "yes" : "no"), "\n";
}

if ($exit !== 0) {
    echo "stderr: ", trim($stderr), "\n";
}
?>
--EXPECT--
exit: 0
lines: 1
id-prefix: elementwise-add-
engine: numphp
median-positive: yes
