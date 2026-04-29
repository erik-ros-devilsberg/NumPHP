# Story 13: Documentation & Community Seeding

> Part of [Epic: NumPHP](epic-numphp.md)

**Outcome:** Extension is discoverable and drives ecosystem adoption.

This story runs as a **parallel marketing track** — it does not need to wait for Story 12 to finish, and most of it can run alongside engineering sprints once the public API has stabilised (post-Story 8).

## Documentation
- API reference in php.net format — legitimises NumPHP as a proper extension.
- Getting started guide.
- Port 5–10 canonical NumPy tutorials to NumPHP equivalents — gives Python developers familiar landmarks.

## Community seeding
- PHP internals mailing list — early core team awareness.
- Reddit (r/PHP), Hacker News — broader developer audience.
- RubixML team outreach — they built Tensor, natural allies.
- Benchmark post: NumPHP vs NumPy on common operations.

The benchmark post is the most important marketing asset. Numbers cut through noise.

## Benchmark scope (suggested)
- Element-wise add/multiply on `[10_000, 10_000]` f64
- `matmul` on `[1024, 1024]` f64 (BLAS path)
- Reduction (`sum`) along axis on `[10_000, 10_000]` f64
- `fromArray` / `toArray` round-trip cost (interop overhead is the honest weak spot)

Publish raw numbers, hardware, BLAS variant, and the script. Reproducibility is the credibility multiplier.
