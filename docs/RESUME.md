# Resume Notes — 2026-04-28

A long autonomous build session. Pick up where this left off tomorrow.

## Where we are

**8 of 13 stories shipped.** Build is green at `-Wall -Wextra`; 31 phpt tests passing.

| # | Sprint | Stories | Status |
|---|--------|---------|--------|
| 1 | project-scaffolding-architecture | 01 | ✓ |
| 2 | ndarray-struct-and-creation-api | 02 + 03 | ✓ |
| 3 | indexing-and-slicing | 04 | ✓ |
| 4 | broadcasting-and-elementwise-ops | 05 + 07 | ✓ |
| 5 | shape-manipulation | 06 | ✓ |
| 6 | blas-integration | 08 | ✓ |

**Remaining:** Stories 09 (stats / math), 10 (LAPACK linalg), 11 (interop — phases A/B/C), 12 (PECL), 13 (docs / community).

## Next sprint to start

**Story 09 — Statistical & Mathematical Functions.**

Scope notes from the story file (`docs/user-stories/backlog/09-stats-and-math-functions.md`):
- Reductions with `axis` and `keepdims`: `sum`, `mean`, `min`, `max`, `std`, `var`, `argmin`, `argmax`. Welford's algorithm for `var` / `std`. NaN-aware variants (`nansum`, etc.).
- Element-wise math: `sqrt`, `exp`, `log`, `log2`, `log10`, `abs`, `power`, `clip`, `floor`, `ceil`, `round`, `sort`, `argsort`.
- Depends on the nd-iterator (already shipped in sprint 4).

Suggested next sprint name: `stats-and-math-functions`. May want to combine 09 + 10 if we want a sprint with linalg fallout (Story 10's `norm` and `solve` lean on `sum`/`std`).

## Working state of the build

- Source files: `numphp.c/h`, `ndarray.c/h` (huge — most of the surface area lives here), `ops.c/h` (still a stub), `linalg.c/h` (still a stub), `nditer.c/h`.
- The convention so far has been to put new PHP_METHODs in `ndarray.c` next to siblings, rather than splitting into `ops.c` / `linalg.c`. If `ndarray.c` gets uncomfortable, Story 09 / 10 are a natural moment to pull `ops.c` and `linalg.c` into real homes for their respective methods.
- `materialize_contiguous` (in `ndarray.c`) is the canonical "give me a contiguous copy" helper. `ensure_contig_dtype` adds dtype-recasting to that. Both are reused.
- `numphp_nditer_init` accepts `nop` inputs + 1 output. For pure reductions there is no second input — pass `nop = 1` and the reduction destination as output. The iterator already handles this.
- `read_scalar_at` / `write_scalar_at` (`ndarray.c`) are the typed-load/store helpers used by every cross-dtype copy site.

## Known minor follow-ups (not blocking)

- True buffer-mutating in-place methods (`$a->addInplace($b)` etc.) are deferred. `$a += $b` works via PHP's compound-assignment composition (allocates fresh).
- Multi-axis `slice` (`$a->slice([0, 3], [1, 4])`) deferred; users chain `slice(...)` after `transpose()`.
- Negative `slice` step deferred.
- Boolean / fancy indexing not in v1.
- Strided BLAS (passing `lda` for transposed views instead of copying) deferred.
- 3D+ batched matmul deferred.
- Native int matmul deferred (currently promotes to f64).
- gcov 80%-coverage gate is `continue-on-error` in CI; flip to blocking once Story 10 lands real code in `linalg.c` and `ops.c`.

## Where to read the system

- `docs/system.md` — the keeper. Cross-cutting decisions + a per-sprint "what was learned" entry. Read this first tomorrow.
- `docs/status.md` — sprint table.
- `docs/user-stories/done/` — 8 stories archived, with audit-driven acceptance criteria. The remaining 5 live in `backlog/`.
- `CHANGELOG.md` — version bumps, 0.0.1 through 0.0.6.

## How to resume

```bash
cd /home/arciitek/git/numphp
make CFLAGS="-Wall -Wextra -O2 -g"   # confirm clean build
make test                            # confirm 31/31 green
```

Then `/agile:shape 09-stats-and-math-functions` (or combine with 10 if we want bigger scope this round).

## Known dev-env caveat

`sudo apt-get install` requires manual user input — Claude can't elevate. Toolchain (`php8.4-dev`, `libopenblas-dev`, `liblapack-dev`) is already installed on this machine. `valgrind` is *not* installed locally; CI runs the valgrind lane in GitHub Actions.
