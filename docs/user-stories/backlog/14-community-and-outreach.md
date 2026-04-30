---
story: Community Seeding & Outreach
created: 2026-04-30
---

> Part of [Epic: NumPHP](epic-numphp.md)

## Description

Take the extension public. This is the announcement / outreach phase — separated from Story 13 because the work is human-driven calendar work, not code, and because release-quality docs/tests/benchmarks must be locked down first.

**Pre-condition:** Story 13 is fully done and the human is comfortable that the repo would survive a careful read by a senior NumPy maintainer.

### Channels
- PHP internals mailing list — early core-team awareness; the "we built this in C the right way" angle.
- Reddit r/PHP — broader PHP developer audience; lead with the use case, not the C internals.
- Hacker News — once the benchmark post exists; numbers are the hook.
- RubixML team — they built Tensor (the previous-generation PHP numerical library); natural allies, may want to integrate or co-promote.

### Artifacts to publish
- Benchmark post (drafted in Story 13, polished and published here). Hosted somewhere persistent — project repo, dev.to, or a personal blog — not just a forum reply.
- Announcement post tailored per channel (mailing-list tone ≠ HN tone ≠ Reddit tone).
- A short FAQ for the predictable questions: "why not just use Python?", "why C and not pure PHP?", "how does this compare to Tensor?", "is this production-ready?", "what's the dtype/dtype-promotion story?".

### Sequencing
- Mailing list first — quietest, most technical audience, sets the tone.
- Benchmark post + RubixML outreach next — establishes credibility before public posting.
- Reddit + HN last — the noisy channels; only post once feedback from the quieter ones is incorporated.

## Acceptance Criteria

- Benchmark post is published at a stable URL.
- Mailing-list announcement sent and acknowledged (at least one substantive reply, even if critical).
- Reddit r/PHP post made.
- Hacker News submission made (timing chosen deliberately — weekday morning US time).
- RubixML team contacted; response logged regardless of outcome.
- FAQ document is checked into the repo and linked from the README.

## Notes

- This story is intentionally **not agent-shapeable** as a sprint. The agent can draft posts, draft the FAQ, and prep talking points, but the human posts, replies, and manages the community thread.
- Treat this as event-driven: each channel is a discrete milestone, not a parallel push. If the mailing-list response surfaces a real concern, fix it before posting elsewhere.
