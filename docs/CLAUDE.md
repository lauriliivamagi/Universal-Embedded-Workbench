# docs/ — operator documentation

Context: human-facing operator docs in the [Diataxis](https://diataxis.fr/) structure
(`tutorials/`, `how-to-guides/`, `reference/`, `explanation/`). This set is the
**source-of-truth for how to operate** the workbench, and it is maintained **code-first**
(see [`../AUTHORITY.md`](../AUTHORITY.md)).

## Edit rules

- When code changes behavior, update the affected page here. Where any doc disagrees with the
  shipped code, **the code wins** and the doc must be corrected to match.
- The canonical conventions table in [`README.md`](README.md) (hostnames, ports, pins) is
  authoritative across the doc set — update it there, once.
- Put a page in the right Diataxis bucket: *tutorial* = learn-by-doing, *how-to* = a task
  runbook, *reference* = dry lookup derived from code, *explanation* = why/background.
- Pages carry YAML frontmatter (type/domain/audience/authority/…); match the existing pages
  when adding one.

## Subtrees that are NOT operator docs

- `spec/` — the FSD. **Design intent, non-authoritative.** Don't cite it as proof of behavior,
  and don't "fix" behavior by editing it; fix the code, then these docs.
- `legacy/` — superseded original-author manuals. **Do not edit or cite.**
- `internal/` — working planning artifacts (the `superpowers/` subtree is git-ignored,
  local-only). Not user-facing.
