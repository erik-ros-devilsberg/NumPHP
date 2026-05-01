# Resume Notes — 2026-05-01 (after sprint 15)

Pick up where this left off next session.

## Where we are

**Version 0.0.12.** Pre-release, iterative. Building toward 0.1.0.
The eventual 1.0 lives at the end of the project — battle-tested by
external parties — not at the end of this near-term scope.

**13 sprints shipped.** Build green at `-Wall -Wextra`; 60/60 phpt + 1
cleanly skipped (FFI ext absent) + the doc-snippet harness running 75
fenced ```php blocks on every PR.

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
| 13 | 15-project-layout (Story 15) | 15 | ✓ | 0.0.12 |

**Remaining for v0.1.0 release-quality push:** Story 12 (PECL packaging),
Story 13 Phase C (benchmarks), gcov gate flip to blocking. None are
required before then.

**Post-1.0:** Story 11 Phase C (Arrow IPC), Story 14 (community + outreach).

## What just landed (sprint 15)

Mechanical layout sprint, no behavioural change.

- 15 tracked C/H files moved from the project root to `src/` via
  `git mv` (so `git blame` follows). Files: `numphp.{c,h}`,
  `ndarray.{c,h}`, `ops.{c,h}`, `linalg.{c,h}`, `io.{c,h}`,
  `bufferview.{c,h}`, `nditer.{c,h}`, `lapack_names.h`.
- `config.m4` updated: `PHP_NEW_EXTENSION` lists `src/foo.c` paths;
  `PHP_ADD_INCLUDE([$ext_srcdir/src])` added so cross-file
  `#include "numphp.h"` keeps resolving.
- `LICENSE` added at root — **BSD 3-Clause**, matching NumPy / SciPy /
  pandas / scikit-learn. README's license section updated to point at
  it (was previously a "TBD" placeholder).
- Decisions **28** (sources under `src/`) and **29** (BSD 3-Clause)
  locked in `docs/system.md`.
- Version bumped to **0.0.12**.

**Effect on root:** ~20 tracked files → ~6. The root now holds only
meta files a newcomer reads first: `README.md`, `CHANGELOG.md`,
`LICENSE`, `CLAUDE.md`, `.gitignore`, `config.m4`.

**One factual correction during execution:** I'd recommended MIT
matching what I assumed NumPy used. NumPy is actually BSD 3-Clause
(so are SciPy, pandas, scikit-learn). Switched to BSD 3-Clause to
match user intent ("mit is used by numpy too right?").

## Next pickup — Story 12 (PECL packaging)

Now naturally next. The `src/` layout from this sprint is
`package.xml`-ready (PECL accepts paths). Story 12 unblocks anyone
who wants to `pecl install numphp` — which is the primary "PHP can do
data work" demonstration, since it removes a build barrier.

What Story 12 needs (best-effort outline; shape it properly when you
start):

1. `package.xml` v2 schema describing every shipped file (sources,
   tests, examples, docs, README, CHANGELOG, LICENSE).
2. `pecl package` produces a `.tgz` that installs cleanly via
   `pecl install ./numphp-0.0.12.tgz`.
3. CI step that does the package + install dry run on every PR.
4. PECL channel registration is a separate manual step — out of
   scope; the goal is a packageable artifact.

### How to start the next session

1. Read `docs/user-stories/backlog/12-pecl-packaging.md`.
2. `/agile:shape 12-pecl-packaging` — in main thread, per memory note,
   no subagent.
3. Human-gate the plan, then `/agile:execute`.

Alternative pickup: **Story 13 Phase C (benchmarks).** Reasoning for
doing 12 first: PECL packaging freezes install ergonomics, which a
benchmark reader expects to exist. Reasoning for doing 13C first:
benchmark numbers are the most direct artifact of the project's
thesis ("PHP can do this in milliseconds"). Either is defensible; my
recommendation stays Story 12 first.

## Working state of the build

- Sources now live under `src/`. 15 tracked source files.
- `config.m4` references `src/*.c` paths.
- CI yaml unchanged from sprint 13b — no path updates needed because
  build commands operate from the project root.
- `scripts/coverage.sh` unchanged — gcovr's `--filter 'numphp\.c$'`
  patterns still match (basename, not absolute path).
- 29 architectural decisions captured in `docs/system.md`.

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

- `docs/system.md` — the keeper. 29 decisions + per-sprint "what was
  learned." Read first.
- `docs/status.md` — sprint table.
- `docs/api/` — complete API reference.
- `docs/concepts/` — five concept guides.
- `docs/getting-started.md` — onboarding.
- `docs/cheatsheet-numpy.md` — NumPy ↔ NumPHP.
- `docs/coverage-audit-2026-05-01.md` — the audit that drove sprint 13b.
- `examples/` — runnable scripts demonstrating real workflows.
- `src/` — C source.
- `docs/user-stories/done/` — completed stories.
  `docs/user-stories/backlog/` — remaining (story 11 with phases A+B
  done and C deferred, 12, 13 with phases A+B done and C deferred, 14 new).
- `CHANGELOG.md` — `0.0.1` through `0.0.12`.

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
(set as workflow-level env in `.github/workflows/ci.yml`) so no
`php_test_results_*.txt` reports get auto-saved into the working tree.
