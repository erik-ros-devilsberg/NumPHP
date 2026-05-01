<?php
/**
 * Snippet harness: extract fenced ```php blocks from Markdown documentation
 * and check that each block runs and produces the documented output.
 *
 * Used by tests/100-doc-snippets.phpt. Convention assumed by Phase A authoring:
 *
 *   ```php
 *   <?php
 *   $a = NDArray::zeros([2, 3]);
 *   print_r($a->shape());
 *   ```
 *
 *   ```
 *   Array ( [0] => 2 [1] => 3 )
 *   ```
 *
 * The plain ``` block immediately following a ```php block (with optional
 * blank lines between) is treated as the expected output.
 *
 * Comparison is whitespace-normalised — the docs collapse multi-line print_r
 * output onto a single line for human readability, and the harness must
 * accept that while still catching semantic drift.
 *
 * Snippets in the same file run in a shared scope so tutorial-style docs
 * (getting-started.md) where each block builds on the previous one work.
 * Snippets without an expected output block are run but their output is
 * suppressed; they only have to not throw.
 *
 * A snippet whose code contains "// snippet-test: skip — <reason>" is
 * skipped with the reason recorded.
 *
 * Only user-facing docs are scanned: README.md, docs/api/, docs/concepts/,
 * docs/getting-started.md, docs/cheatsheet-numpy.md. Planning files
 * (docs/user-stories/, docs/sprints/, docs/system.md, docs/RESUME.md, audit
 * docs) are skipped — those contain design sketches that are not always
 * runnable PHP.
 */

/** Files included in the snippet sweep. */
function snippet_files(string $repo_root): array {
    $files = [];
    $candidates = [
        $repo_root . '/README.md',
        $repo_root . '/docs/getting-started.md',
        $repo_root . '/docs/cheatsheet-numpy.md',
    ];
    foreach ($candidates as $c) {
        if (is_file($c)) $files[] = $c;
    }
    foreach (['api', 'concepts'] as $sub) {
        $dir = $repo_root . '/docs/' . $sub;
        if (!is_dir($dir)) continue;
        foreach (scandir($dir) as $entry) {
            if (str_ends_with(strtolower($entry), '.md')) {
                $files[] = "$dir/$entry";
            }
        }
    }
    sort($files);
    return $files;
}

/**
 * Walk Markdown files and extract every ```php / ``` pair.
 *
 * @return array<string, list<array{file:string,line:int,code:string,expected:?string,skip:?string}>>
 *         Keyed by file path; preserves doc order within each file.
 */
function snippet_extract(string $repo_root): array {
    $by_file = [];
    foreach (snippet_files($repo_root) as $path) {
        $lines = file($path, FILE_IGNORE_NEW_LINES);
        if ($lines === false) continue;
        $i = 0;
        $n = count($lines);
        $entries = [];
        while ($i < $n) {
            if (preg_match('/^```php(?:\s|$)/', $lines[$i])) {
                $start_line = $i + 1;
                $i++;
                $code_lines = [];
                while ($i < $n && !preg_match('/^```\s*$/', $lines[$i])) {
                    $code_lines[] = $lines[$i];
                    $i++;
                }
                $code = implode("\n", $code_lines);
                $i++;
                while ($i < $n && trim($lines[$i]) === '') $i++;

                $expected = null;
                if ($i < $n && preg_match('/^```\s*$/', $lines[$i])) {
                    $i++;
                    $exp = [];
                    while ($i < $n && !preg_match('/^```\s*$/', $lines[$i])) {
                        $exp[] = $lines[$i];
                        $i++;
                    }
                    $expected = implode("\n", $exp);
                    $i++;
                }

                $skip = null;
                if (preg_match('#//\s*snippet-test:\s*skip\s*[—-]\s*(.+)#', $code, $m)) {
                    $skip = trim($m[1]);
                }

                $entries[] = [
                    'file' => $path,
                    'line' => $start_line,
                    'code' => $code,
                    'expected' => $expected,
                    'skip' => $skip,
                ];
            } else {
                $i++;
            }
        }
        if ($entries !== []) $by_file[$path] = $entries;
    }
    return $by_file;
}

/** Whitespace-normalise: collapse all whitespace runs to one space and trim. */
function snippet_normalise(string $s): string {
    $s = preg_replace('/\s+/', ' ', $s);
    return trim($s);
}

/** Strip a leading <?php tag from a snippet body. */
function snippet_strip_open_tag(string $code): string {
    return preg_replace('/^\s*<\?php\s*/', '', $code, 1);
}

/**
 * Run all non-skipped snippets in one file together (shared scope).
 *
 * Snippets with an expected block contribute to both the concatenated code
 * and the concatenated expected text. Snippets without an expected block
 * have their output suppressed via output buffering — they only have to
 * not throw.
 *
 * @return array{ok:bool,run:int,skipped:int,error?:string,expected?:string,actual?:string,fail_at?:string}
 */
function snippet_run_file(array $snippets): array {
    $assembled_code = '';
    $assembled_expected = '';
    $run = 0;
    $skipped = 0;
    foreach ($snippets as $s) {
        if ($s['skip'] !== null) {
            $skipped++;
            continue;
        }
        $body = snippet_strip_open_tag($s['code']);
        if ($s['expected'] === null) {
            $assembled_code .= "ob_start();\n$body\nob_end_clean();\n";
        } else {
            $assembled_code .= "$body\n";
            $assembled_expected .= $s['expected'] . "\n";
        }
        $run++;
    }
    if ($run === 0) {
        return ['ok' => true, 'run' => 0, 'skipped' => $skipped];
    }

    ob_start();
    try {
        (function() use ($assembled_code) {
            eval($assembled_code);
        })();
    } catch (\Throwable $e) {
        $partial = ob_get_clean();
        return [
            'ok' => false,
            'run' => $run,
            'skipped' => $skipped,
            'error' => get_class($e) . ': ' . $e->getMessage(),
            'actual' => $partial,
        ];
    }
    $actual = ob_get_clean();
    $exp_norm = snippet_normalise($assembled_expected);
    $act_norm = snippet_normalise($actual);
    if ($exp_norm !== $act_norm) {
        return [
            'ok' => false,
            'run' => $run,
            'skipped' => $skipped,
            'error' => 'output mismatch',
            'expected' => $exp_norm,
            'actual' => $act_norm,
        ];
    }
    return ['ok' => true, 'run' => $run, 'skipped' => $skipped];
}
