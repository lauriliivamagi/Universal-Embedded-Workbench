# Claude Code skills (agent context)

- **Audience:** Claude Code agents (and the developers who maintain them)
- **Deployment target:** loaded in-repo by Claude Code on the laptop; **never** deployed to the Pi
- **Authority:** **derived** — these encode the REST API and operator workflows that live in
  the code (`pi/`) and operator docs (`docs/`)
- **Edit rule:** update a skill only when the API or a documented workflow changes; mirror
  the operator how-to, never invent behavior the workbench doesn't have

This directory is **pinned**: Claude Code auto-discovers skills at exactly `.claude/skills/`,
so it cannot be relocated. Twelve skills drive the workbench (PlatformIO/ESP-IDF lifecycle,
test harness, debug, WiFi/BLE/MQTT, signal generator, integration, FSD writer). Two more
(`saleae-logic-mcp`, `saleae-logic-python`) are an **exception** to the authority/Pi rules
above: they drive a **laptop-hosted** Saleae logic analyzer through Logic 2's own MCP
(`127.0.0.1:10530`) and `logic2-automation` gRPC (`127.0.0.1:10430`) interfaces, so their
source of truth is Saleae's API, not this repo's `pi/` + `docs/`. See the
table in the repo-root [`README.md`](../../README.md#skills-only-installation) for the full
list, and [`../../AUTHORITY.md`](../../AUTHORITY.md) for where skills sit in the source-of-truth
hierarchy.

To install on another dev machine, copy this directory — see *Skills-only installation* in
the root README.
