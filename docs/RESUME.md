# Resume Notes — 2026-04-30

Pick up where this left off next session.

## Where we are

**11 of 13 stories shipped** (Story 11 in two phases of three; Phase C deferred post-1.0). Build green at `-Wall -Wextra`; 53/53 phpt tests passing + 1 cleanly skipped (FFI ext absent). Version `0.0.10`.

| # | Sprint | Stories | Status |
|---|--------|---------|--------|
| 1 | project-scaffolding-architecture | 01 | ✓ |
| 2 | ndarray-struct-and-creation-api | 02 + 03 | ✓ |
| 3 | indexing-and-slicing | 04 | ✓ |
| 4 | broadcasting-and-elementwise-ops | 05 + 07 | ✓ |
| 5 | shape-manipulation | 06 | ✓ |
| 6 | blas-integration | 08 | ✓ |
| 7 | stats-and-math-functions | 09 | ✓ |
| 8 | linear-algebra-module | 10 | ✓ |
| 9 | php-arrays-and-file-io (Story 11 Phase A) | 11A | ✓ |
| 10 | buffer-view (Story 11 Phase B) | 11B | ✓ |

**Remaining for v1:** Story 12 (PECL packaging), Story 13 (docs / examples / tests / benchmarks). Story 14 (community / outreach) lives post-release.
**Deferred post-1.0:** Story 11 Phase C (Arrow IPC) — see `docs/system.md` Open Items.

## Next pickup: Story 13 (docs / examples / tests / benchmarks)

Decided 2026-04-30:

- Tackle Story 13 before Story 12. Docs + benchmarks surface rough API edges *before* packaging freezes the surface; PECL is easier when the API is locked.
- **Story 13 has been split** — community/outreach moved to a new Story 14 (`docs/user-stories/backlog/14-community-and-outreach.md`). Story 13 is now strictly about pre-release quality: API reference, getting-started, concept guides, NumPy↔numphp cheatsheet, runnable examples wired into CI, a coverage-gap test audit (with the gcov gate flipped to blocking), and a reproducible `bench/` directory producing numphp-vs-NumPy numbers.
- The human's bar for moving 13 → 14: "would I be comfortable if a senior NumPy maintainer read every line of this repo today?" Treat this as the release gate, not a marketing sprint.
- Story 11 Phase C remains deferred — install ergonomics for libarrow unresolved, no user demand yet. Logged in `docs/system.md` Open Items.

### How to start the next session

1. Read `docs/user-stories/backlog/13-docs-and-benchmarks.md` — confirm acceptance criteria still match project state.
2. `/agile:shape 13-docs-and-benchmarks` to draft the sprint plan. Given the four work streams (docs, examples, tests, benchmarks), the shaping conversation should consider whether to ship as one sprint or split into phases (A: docs+cheatsheet, B: examples+tests, C: benchmarks).
3. Human-gate the plan, then `/agile:execute`.

If you want to flip the order and do Story 12 (PECL) first, the pickup is the same — shape `12-pecl-packaging` instead.

## Working state of the build

- Source files: `numphp.c/h`, `ndarray.c/h` (~2500+ lines; the main surface), `ops.c/h` (Sprint 9), `linalg.c/h` (Sprint 10), `io.c/h` (Sprint 11A), `bufferview.c/h` (Sprint 11B), `nditer.c/h`, `lapack_names.h` (LAPACK symbol aliasing for macOS Accelerate). `config.m4` source list reflects all of these.
- LAPACK symbol probe: `config.m4` tries both `dgetri_` and `dgetri`, defines `NUMPHP_LAPACK_NO_USCORE` if needed; `lapack_names.h` aliases the 14 LAPACK symbols.
- Promoted exports in `ndarray.h`: `numphp_read_scalar_at`, `numphp_write_scalar_at`, `numphp_materialize_contiguous`, `numphp_ensure_contig_dtype`, plus the buffer/zval helpers. Backward-compat macros at top of `ndarray.c`.
- 24 architectural decisions captured in `docs/system.md`. Most recent: 22-24 for BufferView (refcount lifecycle, `writeable` advisory in v1, internal-class property defaults).

## Known minor follow-ups (not blocking)

(Carried over from prior RESUME — most still apply.)

- True buffer-mutating in-place methods (`$a->addInplace($b)` etc.) deferred.
- Multi-axis `slice` deferred; users chain `slice()` after `transpose()`.
- Negative `slice` step deferred.
- Boolean / fancy indexing not in v1.
- Strided BLAS (passing `lda` for transposed views instead of copying) deferred.
- 3D+ batched matmul deferred.
- Native int matmul deferred (currently promotes to f64).
- gcov 80%-coverage gate still `continue-on-error` in CI; revisit when Story 13 lands.
- BufferView `writeable=false` is advisory in v1 (does not actually clear `WRITEABLE` on the source NDArray).

## Where to read the system

- `docs/system.md` — the keeper. Cross-cutting decisions + per-sprint "what was learned." Read first.
- `docs/status.md` — sprint table.
- `docs/user-stories/done/` — completed user stories. `docs/user-stories/backlog/` — remaining (11 with phases A/B done and C deferred, 12, 13).
- `CHANGELOG.md` — `0.0.1` through `0.0.10`.

## How to resume

```bash
cd /home/arciitek/git/numphp
phpize && ./configure && make CFLAGS="-Wall -Wextra -O2 -g"   # clean build
make test                                                       # 53/53 + 1 skipped (FFI)
```

Then read `docs/user-stories/backlog/13-*.md`, run `/agile:shape 13-<slug>`, human gate, execute.

## Known dev-env caveat

`sudo apt-get install` requires manual user input — Claude can't elevate. Toolchain (`php8.4-dev`, `libopenblas-dev`, `liblapack-dev`) already installed. `valgrind` not installed locally; CI runs the valgrind lane in GitHub Actions.

## Phase C deferral — context if revisiting

(Captured here so the unfinished thread isn't lost. **Not the next pickup.**)

We were shaping Phase C (Arrow IPC) when the discussion turned into a project-policy debate about install ergonomics:

- libarrow C++ is **not in Ubuntu 24.04 noble's universe**.
- Apache's official APT install method (`wget …apache-arrow-apt-source-latest-noble.deb && dpkg -i`) is "trust on first use" — the .deb's signature isn't verified before install. User pushed back on this as unsafe.
- Agreed dependency principle: external deps must be installable via standard package-manager mechanisms with cryptographic verification (signed-by keyring + `sources.list.d`). Acceptable: distro repos, well-known maintainer PPAs (Ondřej for PHP), upstream-project repos (Apache for Arrow). Not acceptable: `curl … | sh`, lone `.deb` downloads with TLS-only trust.
- User attempted the signed-keyring method on dev machine; `apt update` failed with `does not have a Release file` — doubled `ubuntu` in the URI suggests stray sources file from a prior attempt or wrong URI. Debug commands queued but not run:
  ```bash
  cat /etc/apt/sources.list.d/apache-arrow.sources
  ls /etc/apt/sources.list.d/ | grep -i arrow
  curl -I https://apache.jfrog.io/artifactory/arrow/ubuntu/dists/noble/Release
  ```

**If revisiting**, the standing recommendation was to **vendor nanoarrow** (single-file C, permissive license, IPC-scoped) rather than fight the libarrow install path. Matches the project's "self-contained C extension" style and Phase C's scope (1D/2D primitive dtypes per the user story). User had not yet committed.

When picked up: write the dependency principle as **decision 25** in `docs/system.md` first, then shape `docs/sprints/11c-arrow-ipc.md`, then human gate before executing.
