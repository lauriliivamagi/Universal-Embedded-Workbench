---
type: how-to-guide
domain: complicated
audience: operator
stability: tactical
authority:
  provenance: institutional
  verifiability: executable
  evidence: strong
  currency: dated
epistemic-layer: method
---

# Debug with GDB

Attach a source-level debugger over JTAG to a board in a slot — set breakpoints,
step code, read memory, halt and reset. Reach for this when serial logging isn't
enough and you need to stop the CPU and look inside.

## Check whether a slot is already debuggable

For native-USB-JTAG chips (ESP32-C3/C6/H2/S3), OpenOCD starts **automatically** on
hotplug and at boot — no config, no manual start. Ask the API which slots are
ready:

```bash
curl -s http://pi4b.local:8080/api/devices \
  | jq '.slots[] | {label, debugging, debug_chip, debug_gdb_port}'
```

A slot in state `debugging` with a `debug_gdb_port` (e.g. `3333`) has a live
OpenOCD session you can connect to right now. Per the canonical port map, `SLOT1`
gets GDB port `3333`, telnet `4444`, and serial `4001`; each further slot is
`+1`.

## Pick your debug approach

| Approach | Chips | Hardware | Setup |
|---|---|---|---|
| **Native USB-JTAG** | ESP32-C3 / C6 / H2 / S3 | None — one USB cable | Auto-starts on hotplug; nothing to configure. |
| **Dual-USB** | ESP32-S3 with two USB ports | Two USB cables | JTAG on one port, UART on the other; monitor and debug fully in parallel. |
| **ESP-Prog / FT2232H** | Classic ESP32, ESP32-S2 (no native JTAG) | External probe (one hub port) | Declare under `debug_probes[]` in `workbench.json` and wire TCK/TDI/TDO/TMS. |

For the external-probe path, declare the probe in
[`workbench.json`](../reference/configuration-files.md) and wire it per
[Hardware and wiring → JTAG debug probe wiring](../reference/hardware-and-wiring.md#jtag-debug-probe-wiring-classic-esp32--s2).

## Control a debug session manually (optional)

You normally don't need this for native-USB chips — but you can start, stop, and
inspect sessions explicitly:

```bash
# Start. Omit "slot"/"chip" and the workbench auto-detects both via JTAG TAP-ID.
curl -s -X POST http://pi4b.local:8080/api/debug/start \
  -H 'Content-Type: application/json' \
  -d '{"chip":"esp32c3"}' | jq

# Stop the active session.
curl -s -X POST http://pi4b.local:8080/api/debug/stop \
  -H 'Content-Type: application/json' -d '{}' | jq
```

The start response carries everything you need to connect:

| Field | Meaning |
|---|---|
| `gdb_port` | The GDB remote port (e.g. `3333`). |
| `telnet_port` | The OpenOCD telnet port (e.g. `4444`). |
| `gdb_target` | The exact connect string, with the workbench's detected address filled in (the same `host_ip` as the RFC2217 URLs): `target extended-remote <host_ip>:<gdb_port>`. |

Other endpoints: `GET /api/debug/status` (state of every slot),
`GET /api/debug/probes` (configured ESP-Prog probes),
`GET /api/debug/group` (slot groups for dual-USB configs).

## Connect a debugger

Use the `gdb` that matches the chip family:

| Family | gdb binary |
|---|---|
| ESP32-C3 / C6 / H2 (RISC-V) | `riscv32-esp-elf-gdb` |
| ESP32-S3 (Xtensa) | `xtensa-esp32s3-elf-gdb` |
| ESP32 classic / S2 (Xtensa) | `xtensa-esp32-elf-gdb` |

```bash
riscv32-esp-elf-gdb build/app.elf \
  -ex "target extended-remote pi4b.local:3333" \
  -ex "monitor reset halt"
```

The `gdb_target` field from `/api/debug/start` gives you the exact
`target extended-remote …` string to paste — no need to remember the port.

## Drive OpenOCD over telnet

Each slot has its own OpenOCD telnet port (`SLOT1` = `4444`). Fire commands
without a full gdb session:

```bash
echo "reset run" | nc pi4b.local 4444
```

Useful telnet commands:

| Command | Does |
|---|---|
| `halt` | Stop the CPU. |
| `reset halt` | Reset and stop at the entry point. |
| `reset run` | Reset and let the firmware run. |
| `reg pc` | Read the program counter. |
| `mdw <addr> <count>` | Read `<count>` 32-bit words from `<addr>`. |
| `bp <addr> 2 hw` | Set a hardware breakpoint at `<addr>`. |
| `resume` | Continue execution. |

## Reset while debugging

`POST /api/serial/reset` is debug-aware: when a debug session is active on the
slot it issues a JTAG `reset run` instead of a DTR/RTS pulse, so the board
**does not re-enumerate** on USB and your session survives.

```bash
curl -s -X POST http://pi4b.local:8080/api/serial/reset \
  -H 'Content-Type: application/json' -d '{"slot":"SLOT1"}' | jq
```

## Coexistence and flashing

Serial and JTAG run on **separate USB interfaces**, so on most setups you can
monitor serial and debug at the same time.

The exception is native-USB chips, where serial and JTAG share the **one** USB
connection: **stop debugging before you flash**. The workbench auto-restarts the
debug session once the flash finishes, so you land back in a debuggable state.

Two limits to keep in mind on the **classic ESP32**:

- Only **2 hardware breakpoints** are available.
- Reading memory requires halting the CPU, which can disturb in-flight I2C/SPI
  transactions. If a peripheral wedges after a halt, power-cycle the board.

## IDE snippets

**VS Code** (`launch.json`, cppdbg):

```json
{
  "type": "cppdbg",
  "request": "launch",
  "program": "${workspaceFolder}/build/app.elf",
  "miDebuggerServerAddress": "pi4b.local:3333",
  "MIMode": "gdb"
}
```

**PlatformIO** (`platformio.ini`):

```ini
debug_tool = esp-builtin
debug_port = pi4b.local:3333
```

## Gotchas

- **Classic-ESP32 GPIO12/TDI strapping trap.** TDI lands on GPIO12, the
  flash-voltage strap. If it's HIGH at boot the chip selects 1.8 V flash and a
  3.3 V board crashes. Fix by burning the `VDD_SDIO` eFuse to 3.3 V:

  ```bash
  espefuse.py --port 'rfc2217://pi4b.local:4001?ign_set_control' set_flash_voltage 3.3V
  ```

  (See [Hardware and wiring](../reference/hardware-and-wiring.md).)
- **Native-USB chips: stop debugging before flashing** — serial and JTAG share
  the one USB. The workbench restarts the session afterward.
- **Debug won't auto-start while a slot is flapping or recovering.** Settle the
  slot first — see [Recover a stuck or flapping device](recover-a-stuck-or-flapping-device.md).
- Only **2 hardware breakpoints** on classic ESP32; budget them.

## Related

- [Hardware and wiring](../reference/hardware-and-wiring.md)
- [Configuration files](../reference/configuration-files.md)
- [Recover a stuck or flapping device](recover-a-stuck-or-flapping-device.md)
- [REST API](../reference/rest-api.md)
- [Slot states and network ports](../reference/slot-states-and-network-ports.md)
