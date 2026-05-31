# Firmware (DUT test firmware)

- **Audience:** firmware developers
- **Deployment target:** flashed onto ESP32 devices under test — **not** the Pi
- **Authority:** source-of-truth for the test firmware itself
- **Edit rule:** this is runnable C (ESP-IDF), not documentation; change behavior here

ESP32 firmware the workbench flashes onto devices to exercise itself end-to-end. Built by
developers on a laptop; never installed on the Raspberry Pi.

| Directory | What it is |
|-----------|------------|
| `test-firmware/` | Full ESP-IDF test firmware — UDP logging, WiFi provisioning, OTA, BLE NUS, captive-portal HTTP server. The integration template (see the `workbench-integration` skill). |
| `debug-test/` | Minimal per-chip firmware for JTAG/GDB tests, with pre-built binaries in `debug-test/output/<chip>/` consumed by the `pytest/` WT-18xx suite. |

See [`../AUTHORITY.md`](../AUTHORITY.md) for how this relates to the rest of the repo.
