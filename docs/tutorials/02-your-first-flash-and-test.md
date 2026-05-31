---
type: tutorial
domain: complicated
audience: operator
stability: structural
authority:
  provenance: institutional
  verifiability: executable
  evidence: strong
  currency: dated
epistemic-layer: method
---

# Tutorial 2 — Your first flash and test

The workbench is built and answering at `http://pi4b.local:8080` (if not,
do [Tutorial 1](01-build-and-install-the-workbench.md) first). In this tutorial
you'll take an ESP32 from "plugged in" to "running your firmware and printing to
your screen" — without touching the hardware. Five steps:

1. Confirm you can reach the workbench.
2. Find which board is in which slot.
3. Build some firmware.
4. Flash it over HTTP.
5. Watch it boot, and reset it.

> **You need:** an ESP32 board already plugged into the workbench, and an ESP-IDF
> (or PlatformIO) toolchain on your laptop. `curl` and `jq` make the examples
> readable. We use ESP-IDF here; the PlatformIO equivalents are in
> [Flash firmware](../how-to-guides/flash-firmware.md).

---

## Step 1 — Say hello to the workbench

```bash
curl -s http://pi4b.local:8080/api/info | jq .
```

```json
{ "ok": true, "host_ip": "192.168.0.42", "hostname": "pi4b",
  "slots_configured": 3, "slots_running": 1 }
```

If this fails, you can't do anything else — fix connectivity first (use the Pi's
IP if mDNS is unreliable, or set `SERIAL_PI` to it). Everything from here is just
more HTTP to this same host.

## Step 2 — Find your board

List the slots and look at what's present:

```bash
curl -s http://pi4b.local:8080/api/devices \
  | jq '.slots[] | {label, present, state, detected_chip, url}'
```

```json
{ "label": "SLOT1", "present": true, "state": "idle",
  "detected_chip": "esp32c3", "url": "rfc2217://pi4b.local:4001" }
```

That's your target: **SLOT1**, an **esp32c3**, on serial port **4001**. The
`state: idle` means it's ready.

> **Not sure which physical board is `SLOT1`?** Unplug it and re-run the command —
> the slot's `present` flips to `false`. Plug it back; it returns to the **same**
> slot, because slots are tied to the physical jack, not the device. (Why:
> [Slot identity model](../explanation/slot-identity-model.md).)

## Step 3 — Build firmware

Use any ESP-IDF project (the repo's `firmware/test-firmware/` works, or `idf.py
create-project hello`). Set the target to match your board and build:

```bash
source /opt/esp-idf/export.sh        # or wherever your IDF lives
idf.py set-target esp32c3            # match detected_chip from Step 2
idf.py build
```

A successful build leaves the binaries in `build/`:

- `build/bootloader/bootloader.bin`
- `build/partition_table/partition-table.bin`
- `build/<project>.bin` (your app)
- `build/flash_args` ← the file that says which `.bin` goes at which offset

## Step 4 — Flash it over HTTP

The workbench flashes **on the Pi itself** via `POST /api/flash`. You upload the
binaries; the Pi runs `esptool` against the local USB port and manages the serial
proxy for you. This is the path that works reliably from anywhere, even off-LAN.

From inside `build/`, hand it the `flash_args` file plus each binary it names:

```bash
cd build
curl -s -X POST http://pi4b.local:8080/api/flash \
  -F slot=SLOT1 \
  -F chip=esp32c3 \
  -F baud=921600 \
  -F flash_args=@flash_args \
  -F bootloader.bin=@bootloader/bootloader.bin \
  -F partition-table.bin=@partition_table/partition-table.bin \
  -F <project>.bin=@<project>.bin \
  | jq '{ok, returncode}'
```

> **Rule:** each binary's form-field name must equal the **basename** that
> `flash_args` refers to. If `flash_args` says `app.bin`, the field is `-F app.bin=@app.bin`.

```json
{ "ok": true, "returncode": 0 }
```

`ok: true` and `returncode: 0` means esptool wrote and verified successfully. (The
full esptool log is in the `output` field if you want to read it.) If it fails,
the [recovery runbook](../how-to-guides/recover-a-stuck-or-flapping-device.md)
has you covered.

## Step 5 — Watch it boot and reset it

Reset the board and capture its first lines of output in one call:

```bash
curl -s -X POST http://pi4b.local:8080/api/serial/reset \
  -H 'Content-Type: application/json' \
  -d '{"slot":"SLOT1"}' | jq -r '.output[]'
```

You'll see the ESP32 bootloader banner and your app's first prints. To keep
watching without holding the port, poll the passive buffer:

```bash
curl -s "http://pi4b.local:8080/api/serial/output?slot=SLOT1&lines=40" \
  | jq -r '.lines[].text'
```

Or wait for a specific line your firmware prints:

```bash
curl -s -X POST http://pi4b.local:8080/api/serial/monitor \
  -H 'Content-Type: application/json' \
  -d '{"slot":"SLOT1","pattern":"app_main","timeout":10}' | jq '{matched, line}'
```

🎉 **You just flashed and observed a board entirely over the network.** Repeat for
every slot — the workbench handles several boards at once.

---

## Bonus — run the bundled test suite

The repo ships a pytest suite that drives the workbench through its paces. With a
board plugged in:

```bash
pip install -r requirements-dev.txt
pytest pytest/ --wt-url http://pi4b.local:8080 --run-dut
```

Without `--run-dut`, tests that need a real board are skipped, so you can still
exercise the WiFi/signal-gen/instrument paths with nothing plugged in. Full
details: [Run the test suite](../how-to-guides/run-the-test-suite.md).

## Where to go next

You now know the core loop: **identify → build → flash → observe**. Pick the task
you need:

- [Flash firmware](../how-to-guides/flash-firmware.md) — PlatformIO, erase, offsets, fallbacks.
- [Monitor output and read logs](../how-to-guides/monitor-output-and-logs.md) — serial vs UDP logging.
- [Recover a stuck or flapping device](../how-to-guides/recover-a-stuck-or-flapping-device.md) — when a flash goes wrong.
- [Control device GPIO](../how-to-guides/control-device-gpio.md), [Run WiFi tests](../how-to-guides/run-wifi-tests.md), [Debug with GDB](../how-to-guides/debug-with-gdb.md).
- Reference: [REST API](../reference/rest-api.md) · [Slot states](../reference/slot-states-and-network-ports.md).
