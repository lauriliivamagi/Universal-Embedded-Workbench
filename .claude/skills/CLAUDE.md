# .claude/skills/ — Claude Code agent skills

Context: skills that teach Claude Code how to **drive** the workbench. They are **derived**
(see [`../../AUTHORITY.md`](../../AUTHORITY.md)): they encode the REST API and the workflows
that already live in the code (`pi/`) and operator docs (`docs/`).

This directory is **pinned** — Claude Code auto-discovers skills at exactly `.claude/skills/`.
Do not relocate it.

## The rule

- **Never introduce behavior here.** A skill may only orchestrate what the workbench already
  does. If a workflow isn't in `pi/` + `docs/`, add it there first.
- A skill change that reflects an API change must be paired with the matching update to the
  operator how-to in `docs/how-to-guides/`. Keep the human and agent versions in sync.

## Anatomy

- Each skill is `<name>/SKILL.md` with YAML frontmatter: `name` and a `description` whose text
  includes the trigger phrases. Extra material goes in `<name>/references/`.
- `esp-idf-handling/` and `esp-pio-handling/` ship a `discover-workbench.py` that finds the
  workbench and writes an `/etc/hosts` entry. Skills assume the Pi at `pi4b.local` (or
  `$SERIAL_PI`).
- `fsd-writer/` generates FSDs for **user** projects (`<project>-fsd.md`); it is unrelated to
  this repo's own `docs/spec/Embedded-Workbench-FSD.md`.

To install on another machine, copy this directory — see *Skills-only installation* in the
root `README.md`.
