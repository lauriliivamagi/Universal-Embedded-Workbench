# Universal Embedded Workbench

Raspberry Pi-based test instrument for ESP32 firmware: serial proxy (RFC2217), WiFi AP/STA, GPIO control, HTTP relay, all via REST API.

## Source of truth

See [`AUTHORITY.md`](AUTHORITY.md) for the full hierarchy. In short: **code wins**, then
operator docs (`docs/`, maintained code-first), then agent skills (`.claude/skills/`). The
FSD (`docs/spec/Embedded-Workbench-FSD.md`) is design intent, **non-authoritative** for
shipped behavior. This file holds pointers + commands only — **do not restate behavior here
or in the FSD**; document behavior in code, then operator docs.

## Tech Stack

- **Runtime**: Python 3.9+ (Pi), Python 3.11 (devcontainer)
- **Frameworks**: Flask-like HTTP server (portal.py), pyserial (RFC2217), hostapd/dnsmasq (WiFi)
- **Testing**: pytest, ruff, mypy
- **Hardware**: Raspberry Pi 4B in an Argon One M.2 case (eth0 + wlan0), USB hub for serial slots

## Repository layout (context-bounded roots)

Each root carries a README declaring its audience and authority. Top-level map:

| Root | Context | Runs/used | Authority |
|------|---------|-----------|-----------|
| `pi/` | Pi instrument | On the Pi (deployed via `install.sh` + systemd) | Source-of-truth (runtime) |
| `.claude/skills/` | Laptop agent | Loaded in-repo by Claude Code; never deployed | Derived |
| `firmware/` | DUT firmware | Flashed onto ESP32s (`test-firmware/`, `debug-test/`) | Source-of-truth (firmware) |
| `pytest/` | Test harness | On the laptop, against the Pi over HTTP | Source-of-truth (tests) |
| `docs/` | Operator docs | Read; Diataxis tree | Source-of-truth (operation, code-first) |
| `docs/spec/` | Spec | Read | Design intent — non-authoritative |
| `docs/legacy/` | Legacy | Read | Superseded — ignore |
| `.devcontainer/`, `container/` | Dev env | Laptop containers | Derived |

Each context root carries its own `CLAUDE.md` with working guidance for that subtree
(Claude Code loads it when you work there): [`pi/CLAUDE.md`](pi/CLAUDE.md),
[`firmware/CLAUDE.md`](firmware/CLAUDE.md), [`pytest/CLAUDE.md`](pytest/CLAUDE.md),
[`.claude/skills/CLAUDE.md`](.claude/skills/CLAUDE.md), [`docs/CLAUDE.md`](docs/CLAUDE.md).
This root file holds only repo-wide orientation; per-context specifics live in those.

## Commands

```bash
# Install on Pi
cd pi && bash install.sh

# Discover USB slot keys
rfc2217-learn-slots

# Run portal manually
python3 pi/portal.py

# Run tests
pip install -r requirements-dev.txt
pytest pytest/

# Lint
ruff check .          # enforced gate — must stay clean
mypy --strict .        # aspirational — large pre-existing backlog, not yet clean
```

## Code Style

- Python: ruff for linting (enforced, clean), format with ruff; `mypy --strict` is an aspirational goal with a pre-existing backlog
- `snake_case` for functions and variables
- REST API endpoints under `/api/` namespace
- Slot-based identity: TCP ports tied to physical USB connectors, not devices

## Specifications

- `docs/spec/Embedded-Workbench-FSD.md` -- Full functional specification (design intent,
  **non-authoritative** for shipped behavior; for behavior read the code and `docs/`).

## Key Conventions

- Always release GPIO pins after use: `gpio_set(pin, "z")`
- Host: `pi4b.local` (user `pi4b`); `SERIAL_PI` in the devcontainer may pin the
  Pi's current IP. Deploy commands below use the hostname — see `pi/CLAUDE.md`.
- Deploy portal to Pi: `scp pi/portal.py pi4b@pi4b.local:/tmp/portal.py && ssh pi4b@pi4b.local 'sudo cp /tmp/portal.py /usr/local/bin/rfc2217-portal && sudo systemctl restart rfc2217-portal'`
- Deploy debug_controller: `scp pi/debug_controller.py pi4b@pi4b.local:/tmp/ && ssh pi4b@pi4b.local 'sudo cp /tmp/debug_controller.py /usr/local/bin/debug_controller.py && sudo systemctl restart rfc2217-portal'`

Functional behavior (slot auto-detect, flashing, GPIO API, signal generator, WiFi modes, GDB debug, RFC2217 semantics, etc.) lives in the code (`pi/`) and the operator docs (`docs/`); the FSD at `docs/spec/Embedded-Workbench-FSD.md` is the design-intent reference. Don't restate behavior here.

## Gotchas / Do Not

- Do NOT SSH into the Pi to interact with the workbench -- always use the HTTP API at :8080. The `WorkbenchDriver` in `pytest/workbench_driver.py` wraps all API calls. SSH is only for deploying code updates to `/usr/local/bin/rfc2217-portal`.

## Host Access

See `remote-connections` skill for SSH, InfluxDB, Grafana, and Docker details.
