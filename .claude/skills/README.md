# Claude Code skills (agent context)

- **Audience:** Claude Code agents (and the developers who maintain them)
- **Deployment target:** loaded in-repo by Claude Code on the laptop; **never** deployed to the Pi
- **Authority:** **derived** — these encode the REST API and operator workflows that live in
  the code (`pi/`) and operator docs (`docs/`)
- **Edit rule:** update a skill only when the API or a documented workflow changes; mirror
  the operator how-to, never invent behavior the workbench doesn't have

This directory is **pinned**: Claude Code auto-discovers skills at exactly `.claude/skills/`,
so it cannot be relocated. Twelve skills drive the workbench (PlatformIO/ESP-IDF lifecycle,
test harness, debug, WiFi/BLE/MQTT, signal generator, integration, FSD writer). See the
table in the repo-root [`README.md`](../../README.md#skills-only-installation) for the full
list, and [`../../AUTHORITY.md`](../../AUTHORITY.md) for where skills sit in the source-of-truth
hierarchy.

To install on another dev machine, copy this directory — see *Skills-only installation* in
the root README.
