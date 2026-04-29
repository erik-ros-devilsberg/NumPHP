# Story 12: PECL Packaging & Distribution

> Part of [Epic: NumPHP](epic-numphp.md)

**Outcome:** `pecl install numphp` works.

This is the first story to introduce `package.xml` — Story 1 deliberately leaves it out so the early build cycle stays minimal.

## Deliverables
- `package.xml` — PECL manifest with version, dependencies (openblas, lapack, optionally arrow), file list, role tags.
- Tagged releases on GitHub — PECL picks them up automatically.
- Pre-built binaries via GitHub Actions:
  - Linux: x86_64, aarch64
  - macOS: x86_64, arm64
  - Windows: x86_64 (uses OpenBLAS Windows builds)
- `numphp.ini` defaults for any INI options registered in `MINIT`.
- Release checklist documented in `CHANGELOG.md` and a `RELEASING.md` runbook.

## Windows note
OpenBLAS ships official Windows builds. LAPACK on Windows is bundled inside OpenBLAS's MSVC artifact. Adds CI complexity, but skipping Windows materially limits adoption — keep it in scope.

## PECL conventions
- Version in `package.xml` matches the git tag (`v0.1.0` ↔ `<release>0.1.0</release>`).
- Stability tags follow PECL convention: `alpha` → `beta` → `stable`.
- API/binary stability declared in the manifest; bumping `<api>` requires a recompile by users.
