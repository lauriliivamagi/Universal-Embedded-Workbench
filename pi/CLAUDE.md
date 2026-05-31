# pi/ — Raspberry Pi instrument runtime

Context: this code **runs on the Pi**. It is the deployable instrument and the
**source-of-truth for runtime behavior** (see [`../AUTHORITY.md`](../AUTHORITY.md)). Document
behavior here in code, then in `docs/` — never only in the root `CLAUDE.md` or the FSD.

## Do not

- **Do NOT SSH into the Pi to operate the workbench.** Use the HTTP API at `:8080`
  (the `WorkbenchDriver` in `../pytest/workbench_driver.py` wraps every call). SSH is **only**
  for deploying code to `/usr/local/bin/`.

## Deploy (only when asked)

```bash
# Portal
scp pi/portal.py pi4b@pi4b.local:/tmp/portal.py && \
  ssh pi4b@pi4b.local 'sudo cp /tmp/portal.py /usr/local/bin/rfc2217-portal && sudo systemctl restart rfc2217-portal'
# Debug controller
scp pi/debug_controller.py pi4b@pi4b.local:/tmp/ && \
  ssh pi4b@pi4b.local 'sudo cp /tmp/debug_controller.py /usr/local/bin/debug_controller.py && sudo systemctl restart rfc2217-portal'
```

Full install on the Pi: `sudo bash install.sh` (system deps + scripts + systemd). Scripts
only, no system changes: `sudo bash install.sh --update`.

## Layout

- `portal.py` — main entry: HTTP server + REST API + proxy supervisor. Phantom-port table
  `_PHANTOM_PORTS_BY_MODEL` lives here.
- Controllers: `wifi_controller.py`, `ble_controller.py`, `mqtt_controller.py`,
  `debug_controller.py`, `signal_generator.py` (+ `si5351.py`/`gpclk.py`/`pe4302.py`/`morse.py`).
- `plain_rfc2217_server.py` — RFC2217 proxy (DTR/RTS passthrough). `bcm_gpio.py` — shared
  `/dev/mem` GPIO primitives.
- `config/*.json` (repo) installs to `/etc/rfc2217/` on the Pi. `udev/`, `systemd/`,
  `scripts/` are wired up by `install.sh`.

## Conventions

- `ruff check .` + `mypy --strict .`; `snake_case`; REST endpoints under `/api/`.
- Slot identity = physical USB connector, not device. Ports: TCP `4000+idx`, GDB `3332+idx`,
  OpenOCD telnet `4443+idx`.
- **Always release GPIO after use:** `gpio_set(pin, "z")` (high-Z input + pull-up).

## Known wrinkles (pre-existing — don't silently "fix" as part of unrelated work)

- `install.sh` references `cw_beacon.py`, which was retired and no longer exists.
- `c3_reset_test.py` and `serial_proxy.py` are tracked but **not** installed by `install.sh`
  (dev/scratch).
