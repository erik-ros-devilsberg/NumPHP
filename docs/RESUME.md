# Resume Notes — 2026-05-01 (after sprint 13b)

Pick up where this left off next session.

## Where we are

**Version 0.0.11.** Pre-release, iterative. Building toward 0.1.0, not 1.0.
The eventual 1.0 lives at the end of the project — battle-tested by external
parties — not at the end of this near-term scope.

**12 sprints shipped.** Build green at `-Wall -Wextra`; 60/60 phpt + 1
cleanly skipped (FFI ext absent) + 1 doc-snippet-harness test that runs 75
fenced code blocks from user-facing docs on every PR. Just landed: Story 13
Phase B (examples + snippet harness + gap-closure tests + a real segfault
fix in `fromArray`).

| # | Sprint | Stories | Status | Version |
|---|--------|---------|--------|---------|
| 1 | project-scaffolding-architecture | 01 | ✓ | 0.0.1 |
| 2 | ndarray-struct-and-creation-api | 02 + 03 | ✓ | 0.0.2 |
| 3 | indexing-and-slicing | 04 | ✓ | 0.0.3 |
| 4 | broadcasting-and-elementwise-ops | 05 + 07 | ✓ | 0.0.4 |
| 5 | shape-manipulation | 06 | ✓ | 0.0.5 |
| 6 | blas-integration | 08 | ✓ | 0.0.6 |
| 7 | stats-and-math-functions | 09 | ✓ | 0.0.7 |
| 8 | linear-algebra-module | 10 | ✓ | 0.0.8 |
| 9 | php-arrays-and-file-io (Story 11 Phase A) | 11A | ✓ | 0.0.9 |
| 10 | buffer-view (Story 11 Phase B) | 11B | ✓ | 0.0.10 |
| 11 | documentation-pass (Story 13 Phase A) | 13A | ✓ | (no bump — pure docs) |
| 12 | 13b-examples-and-tests (Story 13 Phase B) | 13B | ✓ | 0.0.11 |

**Remaining for v0.1.0 release-quality push:** Story 12 (PECL packaging),
Story 13 Phase C (benchmarks), gcov gate flip to blocking. None are
required before then.

**Post-1.0:** Story 11 Phase C (Arrow IPC), Story 14 (community + outreach).

## What just landed (sprint 13b)

- `examples/` with 5 runnable scripts: `linear-regression.php`, `kmeans.php`,
  `image-as-array.php`, `time-series.php`, `csv-pipeline.php`. Each has a
  `.expected` file. New CI job diffs them on every PR.
- `tests/100-doc-snippets.phpt` + `tests/_helpers/snippet_runner.php` — every
  fenced ```php block in user-facing docs runs on every PR, output checked
  against the matching ``` block. 75 snippets covered.
- 6 new phpt tests (`055-…` through `060-…`) closing audit gaps.
- **Real bug fixed:** `NDArray::fromArray([[1, [99]]])` (mixed scalar/array
  siblings) used to segfault. Now throws `\ShapeException` cleanly. Found
  during the audit; regression-locked by `tests/060-`.
- gcov filter widened from `ndarray.c + ops.c` only to all 7 C sources in
  `config.m4`. Gate stays non-blocking for now.
- `scripts/coverage.sh` — local-and-CI-parity coverage runner that cleans
  up after itself.
- `docs/coverage-audit-2026-05-01.md` — written audit (covered /
  newly-covered / deferred sections).
- Doc fix in `docs/api/ndarray.md`: `full()` and `fromArray()` throws-lists
  corrected (the previous claims about `\DTypeException` for non-numeric
  values were wrong — silent PHP cast in both cases, matching NumPy on
  NaN→int).
- `PHP_NUMPHP_VERSION` bumped to `0.0.11`. `CHANGELOG.md` entry added.

## Mid-sprint course correction (recorded so the lesson sticks)

Sprint 13b drifted toward release-engineering polish (blocking coverage
gate at 80%, the `floor(actual/5)*5 - 5` threshold rule, ratifying tooling
choices as architectural decisions, an artifact-upload step). User pulled
me back twice: first to flag the drift, then to point out we're at ~0.0.15,
not v1.0 — release-quality CI gates belong at release time, not now.
Re-narrowed: keep Phase B about exercising the API and protecting the docs;
leave gate-flipping for the release-quality sprint that follows Story 12.

The drift is recorded as a project-memory note so future sessions don't
repeat it.

## Next pickup — two viable choices

**Option A: Story 12 (PECL packaging).** PECL packaging changes nothing
about user behaviour — purely a distribution format. Unblocks anyone who
wants to `pecl install numphp` (which is the primary "PHP can do data
work" demonstration, since it removes a build barrier). Doesn't require
any prior work to be polished beyond what we have. Probably 1 sprint.

**Option B: Story 13 Phase C (benchmarks).** Numbers vs NumPy. Highest
leverage for the project's core thesis — "PHP can do this, here's the
proof in milliseconds." But also the loudest artifact, so getting the
methodology right (warm-up runs, hardware fingerprint, BLAS variant,
median + IQR, reproducible script) matters. Probably 1 sprint.

**Recommendation: Story 12 first, then 13C.** Reasoning: PECL packaging
freezes the build/install ergonomics, which is what someone running
benchmarks first does anyway. Doing 13C before 12 means the benchmark
post would have to either (a) rebuild from source for every reader, or
(b) document a packaging story that doesn't exist yet. Neither is great.

### How to start the next session

1. Read `docs/user-stories/backlog/12-pecl-packaging.md`.
2. `/agile:shape 12-pecl-packaging` (in main thread, per memory note —
   no subagent for shaping).
3. Human-gate the plan, then `/agile:execute`.

If you'd rather do 13C first: `/agile:shape 13-docs-and-benchmarks` and
scope to **Phase C only**.

## Working state of the build

- Source files changed this sprint: `ndarray.c` (one targeted fix in
  `fromarray_walk` — added `rank_locked` flag).
- New files: `examples/**` (with `.expected` siblings), `tests/_helpers/
  snippet_runner.php`, `tests/100-doc-snippets.phpt`, `tests/055-…` through
  `tests/060-…`, `scripts/coverage.sh`, `docs/coverage-audit-2026-05-01.md`.
- Modified configuration: `.github/workflows/ci.yml` (added `examples`
  job, widened gcov filter, added workflow-level `NO_INTERACTION: true`).
- 29 architectural decisions captured in `docs/system.md` (decisions 28-29
  *not* added — that was deliberate scope correction; the coverage
  threshold and snippet-test convention are tooling choices, not
  architecture).

## Known minor follow-ups (not blocking, deferred from earlier sprints)

- True buffer-mutating in-place methods (`$a->addInplace($b)` etc.) deferred.
- Multi-axis `slice` deferred; users chain `slice()` after `transpose()`.
- Negative `slice` step deferred.
- Boolean / fancy indexing not in v1.
- Strided BLAS (passing `lda` for transposed views instead of copying) deferred.
- 3D+ batched matmul deferred.
- Native int matmul deferred (currently promotes to f64).
- gcov gate still `continue-on-error: true`; flip when v0.1.0 release prep starts.
- BufferView `writeable=false` is advisory in v1 (does not actually clear
  `WRITEABLE` on the source NDArray).
- Story 11 Phase C (Arrow IPC): vendoring nanoarrow is the standing
  recommendation when revisiting; user had not committed.

## Where to read the system

- `docs/system.md` — the keeper. 27 decisions + per-sprint "what was
  learned." Read first.
- `docs/status.md` — sprint table.
- `docs/api/` — complete API reference.
- `docs/concepts/` — five concept guides.
- `docs/getting-started.md` — onboarding.
- `docs/cheatsheet-numpy.md` — NumPy ↔ NumPHP.
- `docs/coverage-audit-2026-05-01.md` — the audit that drove sprint 13b.
- `examples/` — runnable scripts demonstrating real workflows.
- `docs/user-stories/done/` — completed stories.
  `docs/user-stories/backlog/` — remaining (story 11 with phases A+B done
  and C deferred, 12, 13 with phases A+B done and C deferred, 14 new).
- `CHANGELOG.md` — `0.0.1` through `0.0.11`.

## How to resume

```bash
cd /home/arciitek/git/numphp
phpize && ./configure && make CFLAGS="-Wall -Wextra -O2 -g"   # clean build
NO_INTERACTION=true make test                                   # 60/60 + 1 skipped (FFI)
```

## Outstanding ops

- **No remote configured.** Local commits only. Set up a remote with
  `git remote add origin <url>` then `git push -u origin main` when ready
  to publish.

## Known dev-env caveat

`sudo apt-get install` requires manual user input — Claude can't elevate.
Toolchain (`php8.4-dev`, `libopenblas-dev`, `liblapack-dev`, `gcovr`)
already installed. `valgrind` not installed locally; CI runs the valgrind
lane in GitHub Actions.

`make test` interactive prompt: always run with `NO_INTERACTION=true`
(now set as a workflow-level env in `.github/workflows/ci.yml` too) so
no `php_test_results_*.txt` reports get auto-saved into the working tree.
