---
type: reference
domain: clear
audience: operator
stability: structural
authority:
  provenance: institutional
  verifiability: auditable
  evidence: strong
  currency: dated
epistemic-layer: practice
---

# Universal Embedded Workbench — Operator Documentation

The workbench is a **Raspberry Pi 4B** (in an Argon One M.2 case) that turns a
pile of ESP32 boards into a network test instrument. Every board plugged into it
becomes a **slot** you drive over HTTP: flash it, watch its serial output, reset
it, recover it when it bricks, test its WiFi/BLE, feed it an RF signal, or attach
a debugger — all from your computer, none of it requiring you to touch the
hardware again.

This documentation is **operator-focused**: how to build the rig, set it up, run
it day-to-day, and get a stuck device back to life. It is organized using the
[Diataxis](https://diataxis.fr/) framework — four kinds of document, each
answering a different question.

> **New here? Start with the [tutorials](#-tutorials-learn-by-doing).** Build the
> rig, then flash your first board. Everything else is lookup material you reach
> for once you know the shape of the system.

---

## Canonical conventions (true everywhere in these docs)

These hold across every page. When an older doc or the FSD disagrees, **this
table and the code win**.

| Thing | Value | Notes |
|---|---|---|
| Host name | `pi4b.local` | Set the Pi hostname to `pi4b`. Override with the `SERIAL_PI` env var if you use a fixed IP. |
| Portal / API | `http://pi4b.local:8080` | All control goes through here. **Never SSH in to operate the workbench** — SSH is only for deploying code. |
| Authentication | **None** | The API is wide open on the LAN. Keep the workbench on a trusted network. See [Architecture overview](explanation/architecture-overview.md#security-posture). |
| Slot labels | `SLOT1`, `SLOT2`, … | A slot is a **physical USB port** (a Pi jack, or a port on an attached hub), not a device. Same jack → same slot, always. |
| Serial (RFC2217) port | `4001 + (slot index − 1)` | `SLOT1` → `4001`, `SLOT2` → `4002`. |
| GDB port | `3333 + (slot index − 1)` | `SLOT1` → `3333`. |
| OpenOCD telnet port | `4444 + (slot index − 1)` | `SLOT1` → `4444`. |
| UDP log receiver | `5555/udp` | ESP32 firmware sends logs here. |
| Discovery beacon | `5888/udp` | Answers `DISCOVER` probes so clients can find the workbench. |
| DUT reset pin | Pi BCM **GPIO17** → EN/RST (active LOW) | Optional wiring. |
| DUT boot pin | Pi BCM **GPIO18** → GPIO0 (ESP32) / GPIO9 (C3) (active LOW) | Optional wiring. |
| GPIO release value | `"z"` | High-impedance **input with pull-up** — the pull-up safely holds active-low straps de-asserted. |
| Preferred flashing | `POST /api/flash` | Pi-side esptool. Works from anywhere, including off-LAN. |

---

## 📚 Tutorials (learn by doing)

Start-to-finish walkthroughs. Follow them in order; they assume nothing.

| Guide | For whom |
|---|---|
| [1 — Build and install the workbench](tutorials/01-build-and-install-the-workbench.md) | The person provisioning the rig: parts → cabling → connect to your LAN → `install.sh` → verify it's alive. |
| [2 — Your first flash and test](tutorials/02-your-first-flash-and-test.md) | The person using the rig: connect → identify which board is in which slot → flash → watch serial → run a test. |

## 🔧 How-to guides (get a task done)

Goal-oriented runbooks. You know the system; you need the steps for one job.

| Guide | When you need to… |
|---|---|
| [Flash firmware](how-to-guides/flash-firmware.md) | Put a build onto a board (ESP-IDF, PlatformIO, erase, fallbacks). |
| [Monitor output and read logs](how-to-guides/monitor-output-and-logs.md) | See what a board is printing — serial, UDP, or the activity log. |
| [Recover a stuck or flapping device](how-to-guides/recover-a-stuck-or-flapping-device.md) | **Runbook.** A board is bricked, crash-looping, or flapping the USB bus. |
| [Control device GPIO](how-to-guides/control-device-gpio.md) | Drive the Pi's GPIO pins wired to a board (reset, boot-mode, custom). |
| [Run WiFi tests](how-to-guides/run-wifi-tests.md) | Use the Pi as an AP/station, scan, relay HTTP, provision a captive portal. |
| [Use the signal generator](how-to-guides/use-the-signal-generator.md) | Emit an RF carrier or Morse beacon, set attenuation. |
| [Debug with GDB](how-to-guides/debug-with-gdb.md) | Attach a source-level debugger over JTAG. |
| [Test BLE devices](how-to-guides/test-ble-devices.md) | Scan, connect, and write to a BLE peripheral. |
| [Run the test suite](how-to-guides/run-the-test-suite.md) | Execute the pytest end-to-end suite against the workbench. |
| [Maintain and update the workbench](how-to-guides/maintain-and-update-the-workbench.md) | Deploy code updates, restart the service, read logs, add a Pi model. |

## 📖 Reference (look it up)

Dry, complete, authoritative. Derived from the code, not the spec.

| Page | Contains |
|---|---|
| [REST API](reference/rest-api.md) | Every HTTP endpoint, its fields, and its responses. |
| [Hardware and wiring](reference/hardware-and-wiring.md) | Bill of materials, the Argon One M.2 case, pinouts, supported chips, signal-gen wiring. |
| [Slot states and network ports](reference/slot-states-and-network-ports.md) | The slot state machine and the full port map. |
| [Configuration files](reference/configuration-files.md) | `workbench.json` and `signalgen.json` schemas and locations. |

## 💡 Explanation (understand why)

Background that makes troubleshooting make sense. Read when something surprises you.

| Page | Explains |
|---|---|
| [Architecture overview](explanation/architecture-overview.md) | The subsystems, the eth0/wlan0 split, operating modes, security posture. |
| [Slot identity model](explanation/slot-identity-model.md) | Why slots are tied to physical USB connectors, not devices. |
| [Serial reset and download mode](explanation/serial-reset-and-download-mode.md) | The DTR/RTS download-mode trap and why flashing needs `?ign_set_control`. |
| [Crash recovery and flapping](explanation/crash-recovery-and-flapping.md) | How the workbench rescues a board that's cycling the USB bus. |

---

## Related material (not part of this set)

See the repo-root [`AUTHORITY.md`](../AUTHORITY.md) for the full source-of-truth hierarchy.

- [`docs/spec/Embedded-Workbench-FSD.md`](spec/Embedded-Workbench-FSD.md) — the full
  functional specification. Describes *intended* design; **non-authoritative for shipped
  behavior**. Where it diverges from the code, these operator docs (which follow the code) win.
- The Claude Code **skills** under [`.claude/skills/`](../.claude/skills/) (`esp-idf-handling`,
  `workbench-wifi`, `signal-generator`, …) automate these workflows for an AI agent. The
  how-to guides here are the human-readable equivalents.
- [`docs/legacy/`](legacy/) holds the superseded [`User Manual.md`](legacy/User%20Manual.md)
  and [`WiFi-Workbench-HTTP-Manual.md`](legacy/WiFi-Workbench-HTTP-Manual.md), which predate
  this set and use older host names (`serial1`, `192.168.0.87`). Non-authoritative — prefer
  the pages above.
