# Source of Truth

This repo serves four contexts (Pi instrument, laptop agent/dev, operator docs, legacy).
When two of them describe the same behavior and disagree, resolve the conflict with the
hierarchy below — **higher wins**.

## Hierarchy (highest wins)

1. **Code** — `pi/*.py` (runtime), `firmware/**` (DUT firmware), `pytest/**` (test
   behavior). The running system is the final authority. If a doc disagrees with the code,
   the doc is wrong.
2. **Operator docs** — `docs/` (the Diataxis tree). Maintained **code-first**: when code
   changes, these change. Authoritative for *how to operate* the workbench; defers to the
   code on behavior.
3. **Agent skills** — `.claude/skills/`. Derived from the code and the operator docs. They
   must **not** introduce behavior that isn't in (1) or (2).
4. **FSD** — `docs/spec/Embedded-Workbench-FSD.md`. Describes *intended* design.
   **Non-authoritative for shipped behavior** and known to diverge (it still describes a Pi
   Zero W; the production build is a Pi 4B + Argon One M.2). Keep it as design intent; never
   cite it as proof of how the system behaves.
5. **CLAUDE.md** — agent orientation only: pointers, the repo map, deploy commands, and
   "do not" rules. Holds **no** behavioral spec — it points at (1), (2), and (4).
6. **Legacy** — `docs/legacy/`. Superseded original-author material. Ignore for anything
   current.

## Edit-routing rule

A behavior change flows **code → operator docs → skills**. Never document behavior *only* in
CLAUDE.md or the FSD — those are pointers and intent, not the system of record. If you find
yourself about to "fix" the FSD or CLAUDE.md to change how the workbench behaves, you are
editing the wrong file: change the code, then the operator docs.

## Per-root authority at a glance

| Root | Authority |
|------|-----------|
| `pi/` | Source-of-truth (runtime) |
| `firmware/` | Source-of-truth (DUT firmware) |
| `pytest/` | Source-of-truth (test behavior) |
| `docs/` (Diataxis) | Source-of-truth (operation), code-first |
| `.claude/skills/` | Derived |
| `docs/spec/` | Design intent — non-authoritative |
| `docs/legacy/` | Superseded — ignore |
| `CLAUDE.md` | Orientation/pointers only |
