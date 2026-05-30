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

# Control device GPIO

Drive the Pi's GPIO pins wired to a board under test — pulse a reset line, force
download mode, or toggle a custom signal. Use this when you have jumper wires
between the Pi's 40-pin header and the DUT and you want scripted hardware
control over HTTP.

## The two endpoints

Everything runs through `http://pi4b.local:8080`. No SSH, ever.

Set a pin:

```bash
curl -s -X POST http://pi4b.local:8080/api/gpio/set \
  -H 'Content-Type: application/json' \
  -d '{"pin":18,"value":0}'
# → {"ok":true,"pin":18,"value":0}
```

`value` is `0` (drive LOW), `1` (drive HIGH), or `"z"` (release — input with
pull-up).

Read the pins you are currently driving:

```bash
curl -s http://pi4b.local:8080/api/gpio/status | jq .
# → {"ok":true,"pins":{"18":{"direction":"out","value":0}}}
```

**Allowed pins: 16–27.** Anything else returns `400`. A `value` other than
`0`/`1`/`"z"` also returns `400`.

## Always release a pin to `"z"` when done

A pin left driven LOW pins the board in reset or download mode. When you finish
with a pin, return it to `"z"` — high-impedance input with a pull-up. The
pull-up holds active-low straps **de-asserted** (HIGH), so the board boots
normally.

Use a try/finally shape so the release always happens, even if your work fails:

```bash
set_pin() { curl -s -X POST http://pi4b.local:8080/api/gpio/set \
  -H 'Content-Type: application/json' -d "{\"pin\":$1,\"value\":$2}" >/dev/null; }

trap 'set_pin 18 "\"z\""' EXIT   # always release on exit

set_pin 18 0          # assert
# ... do your work ...
# trap releases pin 18 to "z" no matter how this script ends
```

A pin released to `"z"` **disappears from `/api/gpio/status`** — that is
expected, not a failure. `status` only lists pins the workbench is actively
driving.

## The standard DUT pins

If you followed the optional reset/boot wiring, two pins matter:

| Pi GPIO (BCM) | DUT pin | Function | Asserted state |
|---|---|---|---|
| **GPIO17** | EN / RST | Hardware reset | LOW |
| **GPIO18** | GPIO0 (ESP32) / GPIO9 (C3) | Boot-mode select (download mode) | LOW |

These are `gpio_en` (17) and `gpio_boot` (18) in
[`workbench.json`](../reference/configuration-files.md). Both are **active LOW**.

## Pattern: pulse a pin

Assert briefly, then release:

```bash
curl -s -X POST http://pi4b.local:8080/api/gpio/set -d '{"pin":17,"value":0}'
# brief delay handled by your tooling, then release
curl -s -X POST http://pi4b.local:8080/api/gpio/set -d '{"pin":17,"value":"z"}'
```

## Pattern: force download mode by hand

To put a classic ESP32 into download (flash) mode manually — BOOT held LOW
across an EN reset:

1. Drive **BOOT (18) LOW**.
2. Drive **EN (17) LOW** for ~0.2 s, then set **EN (17) HIGH**.
3. Release **BOOT (18) to `"z"`**.

```bash
S=http://pi4b.local:8080/api/gpio/set
curl -s -X POST $S -d '{"pin":18,"value":0}'   # BOOT low
curl -s -X POST $S -d '{"pin":17,"value":0}'   # EN low (reset)
# ~0.2 s
curl -s -X POST $S -d '{"pin":17,"value":1}'   # EN high — boot with BOOT still low
curl -s -X POST $S -d '{"pin":18,"value":"z"}' # release BOOT
```

The workbench does this for you automatically during USB-flap recovery — see
[Crash recovery and flapping](../explanation/crash-recovery-and-flapping.md). For
the managed recovery runbook, see
[Recover a stuck or flapping device](recover-a-stuck-or-flapping-device.md).

## Reset gotcha: release strap pins before `/api/serial/reset`

`/api/serial/reset` fires a DTR/RTS pulse on the serial port. If a Pi pin is
wired to a boot-strap pin (BOOT/EN) and you have it **driven**, it will float or
sit LOW during that pulse and can drop the board into the wrong mode. Set strap
pins to `"z"` **before** calling reset:

```bash
curl -s -X POST http://pi4b.local:8080/api/gpio/set -d '{"pin":18,"value":"z"}'
curl -s -X POST http://pi4b.local:8080/api/serial/reset -d '{"slot":"SLOT1"}'
```

## Trust the response, don't poll to verify

A `200` with `{"ok":true}` means the pin was set. Don't poll
`/api/gpio/status` to "confirm" it — and especially don't expect a released
(`"z"`) pin to show up there, because it won't.

## Gotchas

- Pins outside **16–27** → `400`. Only 16–27 are general-purpose here.
- Pins **5/6** (GPCLK) and **12/13** (PE4302) are reserved for the signal
  generator; **2/3** are I²C. That's why they're excluded — see
  [pin conflicts](../reference/hardware-and-wiring.md#pin-conflicts-you-must-respect).
- **Pi GPIO is 3.3 V only.** Wire only to 3.3 V boards.
- `"z"` is the safe parked state, not `1`. The pull-up — not a driven HIGH —
  holds active-low straps de-asserted.

## Related

- [Recover a stuck or flapping device](recover-a-stuck-or-flapping-device.md)
- [Serial reset and download mode](../explanation/serial-reset-and-download-mode.md)
- [Hardware and wiring](../reference/hardware-and-wiring.md)
- [Configuration files](../reference/configuration-files.md)
- [REST API](../reference/rest-api.md)
