---
type: how-to-guide
domain: chaotic
audience: operator
stability: tactical
authority:
  provenance: institutional
  verifiability: testable
  evidence: moderate
  currency: dated
epistemic-layer: method
---

# Recover a stuck or flapping device

A board can wedge the slot — corrupt flash, a boot loop, or a UART stuck in the
download prompt. **Stabilize first, diagnose later.** For the mechanism behind
flapping and auto-recovery, see
[Crash recovery and flapping](../explanation/crash-recovery-and-flapping.md).

## Symptom → action

| Symptom | Action |
|---|---|
| `state` is `flapping` / `recovering`; log says "flapping detected" | The workbench is auto-recovering. Wait 10–80 s; trigger manually with `POST /api/serial/recover`. |
| `state` is `download_mode` | Flash known-good firmware, then `POST /api/serial/release`. |
| Blank / bricked flash, OTA impossible | Serial-flash known-good firmware; force download mode if needed (see below). |
| ESP32-C3 prints "waiting for download" / "boot:0x7" | `POST /api/serial/reset` — only a system reset re-samples the boot strap. |
| No serial output at all | Check baud 115200, close stale RFC2217 sessions, `POST /api/serial/reset`, else reflash. |
| `flapping` flag lingers after the board recovered | Self-clears on the next `/api/devices` poll once events age out (30 s). |
| Nothing else works | **Last resort:** restart the portal service (the one allowed SSH action). |

## Diagnose first

```bash
curl -s http://pi4b.local:8080/api/devices \
  | jq '.slots[] | {label, state, flapping, recovering, recover_retries, has_gpio, last_error}'
```

Map the `state` you see using
[Slot states and network ports](../reference/slot-states-and-network-ports.md).
`has_gpio: true` means the slot has reset/boot pins wired, which changes how
recovery behaves.

## Flapping

**Symptoms:** `state` is `flapping` or `recovering`; the activity log shows
`flapping detected (N events in 30s)`. **Cause:** corrupt or empty flash, or a
boot loop cycling the USB bus.

The workbench auto-recovers — you usually just wait:

- **GPIO-wired slot** (`has_gpio: true`): unbinds the USB device → waits a 10 s
  cooldown → holds BOOT LOW → pulses EN → rebinds → parks the board in
  `download_mode`. Whole cycle ~10–80 s.
- **No-GPIO slot:** unbind → wait → rebind, retrying up to **2 times**. If that
  fails, the slot stays `flapping` with `last_error` = "needs manual
  intervention".

Trigger recovery manually at any time:

```bash
curl -s -X POST http://pi4b.local:8080/api/serial/recover \
  -H 'Content-Type: application/json' -d '{"slot":"SLOT1"}' | jq '{ok, message}'
```

## Download_mode

After recovery on a GPIO slot, the board sits in `download_mode` with BOOT held
LOW — ready to take a flash.

1. Flash known-good firmware:

   ```bash
   curl -s -X POST http://pi4b.local:8080/api/flash \
     -F slot=SLOT1 -F chip=esp32c3 -F erase=1 \
     -F flash_args=@flash_args \
     -F bootloader.bin=@bootloader/bootloader.bin \
     -F partition-table.bin=@partition_table/partition-table.bin \
     -F my-app.bin=@my-app.bin | jq '{ok, returncode}'
   ```

2. Release the board — drops BOOT to high-Z, pulses EN, returns the slot to
   `idle`:

   ```bash
   curl -s -X POST http://pi4b.local:8080/api/serial/release \
     -H 'Content-Type: application/json' -d '{"slot":"SLOT1"}' | jq .ok
   ```

(See [Flash firmware](flash-firmware.md) for the full `/api/flash` options.)

## Bricked / blank flash

A blank or corrupt flash can't take OTA — you **must** serial-flash. If the
auto-reset doesn't drop the chip into the bootloader:

- **GPIO-wired slot:** force download mode via the boot/reset pins (see
  [Control device GPIO](control-device-gpio.md)), then flash with esptool's
  `--before=no_reset` so it doesn't fight your manual strap.
- **No-GPIO slot:** flash on the Pi with esptool's `--before=usb_reset`, which
  uses the USB reset to enter the bootloader.

Once it takes a good image, recover/release as above to return to `idle`.

## ESP32-C3 stuck in download mode

**Symptom:** serial shows "waiting for download" or `boot:0x7`. A plain line
toggle won't fix it — only a **system/watchdog reset** re-samples the boot strap:

```bash
curl -s -X POST http://pi4b.local:8080/api/serial/reset \
  -H 'Content-Type: application/json' -d '{"slot":"SLOT1"}' | jq -r '.output[]'
```

Background:
[Serial reset and download mode](../explanation/serial-reset-and-download-mode.md).

## Erase flash (clean slate)

```bash
# Simplest: erase as part of the flash
curl -s -X POST http://pi4b.local:8080/api/flash \
  -F slot=SLOT1 -F chip=esp32c3 -F erase=1 -F flash_args=@flash_args \
  -F bootloader.bin=@bootloader/bootloader.bin \
  -F partition-table.bin=@partition_table/partition-table.bin \
  -F my-app.bin=@my-app.bin | jq '{ok, returncode}'
```

On a GPIO slot you can also recover → run `esptool erase_flash` over RFC2217 →
release.

## No serial output

1. Confirm baud is **115200**.
2. Close any stale RFC2217 session (one client max per slot).
3. `POST /api/serial/reset` to capture the boot banner.
4. Still nothing? Reflash.

## If a debug session is active

When the slot is `debugging`, `POST /api/serial/reset` uses a JTAG `reset
run`/halt to stop the loop **at the source** — no USB re-enumeration, so your GDB
session survives.

## Stale flapping flag

If the device has since stabilized but `flapping` is still set, it clears
automatically on the next `/api/devices` poll once the events age out of the
30 s window. No action needed.

## Last resort (the one allowed SSH action)

If a slot is stuck in stale state that nothing above clears, restart the portal
service. **This is the only time SSH is acceptable for operations:**

```bash
ssh pi4b@pi4b.local 'sudo systemctl restart rfc2217-portal'
```

## Related

- [Control device GPIO](control-device-gpio.md)
- [Flash firmware](flash-firmware.md)
- [Crash recovery and flapping](../explanation/crash-recovery-and-flapping.md)
- [Serial reset and download mode](../explanation/serial-reset-and-download-mode.md)
- [Slot states and network ports](../reference/slot-states-and-network-ports.md)
