---
type: reference
domain: clear
audience: operator
stability: structural
authority:
  provenance: canonical
  verifiability: testable
  evidence: strong
  currency: dated
epistemic-layer: practice
---

# Slot states and network ports

Two lookup tables you'll reach for constantly: what each slot `state` means, and
which port belongs to which slot.

## Slot states

Every slot reports a `state` in `GET /api/devices`. These are the only values:

| State | Meaning | What you do |
|---|---|---|
| `absent` | No device on this hub port. | Plug a board in. |
| `idle` | Device present, proxy running, nothing happening. | Ready to flash/monitor. |
| `monitoring` | A serial read is in progress. | Wait, or read `/api/serial/output`. |
| `resetting` | A reset pulse is in flight. | Transient — returns to `idle`. |
| `debugging` | An OpenOCD/JTAG session is active. | Connect GDB; reset uses JTAG. |
| `flapping` | The USB device is connect/disconnect cycling (e.g. corrupt flash, boot loop). | Recovery is starting — see below. |
| `recovering` | The workbench is actively rescuing the slot. | Wait 10–80 s. |
| `download_mode` | Recovery parked the board in the bootloader, BOOT held LOW. | Flash it, then `POST /api/serial/release`. |

The recovery-related flags `flapping`, `recovering`, `recover_retries`,
`has_gpio`, `gpio_boot`, and `gpio_en` also appear per-slot. To understand how a
slot moves between `flapping` → `recovering` → `download_mode`/`idle`, read
[Crash recovery and flapping](../explanation/crash-recovery-and-flapping.md). The
runbook is [Recover a stuck or flapping device](../how-to-guides/recover-a-stuck-or-flapping-device.md).

### Typical lifecycle

```
absent ──plug──▶ idle ──flash/monitor──▶ idle
                  │
                  ├──/api/serial/monitor──▶ monitoring ──▶ idle
                  ├──/api/serial/reset────▶ resetting  ──▶ idle
                  ├──/api/debug/start─────▶ debugging
                  │
        corrupt flash / boot loop
                  ▼
              flapping ──▶ recovering ──▶ download_mode ──flash──▶ /api/serial/release ──▶ idle
                                       └─(no GPIO, retries exhausted)─▶ flapping (manual)
```

## Network ports

Every port the workbench listens on. Per-slot ports increment with the slot index.

| Port | Proto | Purpose | Per slot? |
|---|---|---|---|
| **8080** | TCP/HTTP | Web portal, REST API, firmware downloads | No |
| **4001+** | TCP/RFC2217 | Serial proxy. `SLOT1`→4001, `SLOT2`→4002, … | Yes |
| **3333+** | TCP/GDB | OpenOCD GDB remote. `SLOT1`→3333, … | Yes |
| **4444+** | TCP/telnet | OpenOCD telnet (`reset`, `halt`, `reg`, …). `SLOT1`→4444, … | Yes |
| **5555** | UDP | ESP32 firmware log receiver | No |
| **5888** | UDP | Discovery beacon (answers `DISCOVER` probes) | No |
| 1883 | TCP | Mosquitto broker (installed, **not** portal-controlled) | No |

The per-slot bases come from the `TCP_PORT_BASE` (4001), `GDB_PORT_BASE` (3333),
and `TELNET_PORT_BASE` (4444) environment variables. The exact port for any slot
is in that slot's `tcp_port` / `gdb_port` / `openocd_telnet_port` fields from
`GET /api/devices` — read it from there rather than computing it, since pinned
slots in [`workbench.json`](configuration-files.md) can override the default.

### Reaching a slot's serial port

```bash
# The url field is precomputed for you:
curl -s http://pi4b.local:8080/api/devices | jq -r '.slots[] | "\(.label) \(.url)"'
# SLOT1 rfc2217://pi4b.local:4001
# SLOT2 rfc2217://pi4b.local:4002
```

Always open an RFC2217 URL with `?ign_set_control` from flashing tools, and
pre-set DTR/RTS low — see
[Serial reset and download mode](../explanation/serial-reset-and-download-mode.md).

## Related

- [REST API](rest-api.md) — `/api/devices` field list.
- [Configuration files](configuration-files.md) — pinning ports per slot.
