---
type: explanation
domain: complicated
audience: operator
stability: foundational
authority:
  provenance: institutional
  verifiability: auditable
  evidence: strong
  currency: dated
epistemic-layer: theory
---

# Serial reset and download mode

Sooner or later you will open a serial connection to an ESP32-C3 and get
nothing — no boot banner, no log, just silence — even though the board is
clearly enumerated and healthy. The board isn't hung; it's sitting in
**download mode** because *opening the serial port put it there.* This page
explains the mechanism behind that, and behind the handful of rules the
workbench imposes on flashing (`?ign_set_control`, prefer `/api/flash`, reset to
recover). Once the mechanism is clear, those rules stop looking arbitrary.

## Background: the serial control lines double as reset and boot

ESP32 boards don't have dedicated reset/boot wires on their USB connector. They
reuse the serial port's two control lines — **DTR** and **RTS** — to drive the
chip's two strap pins. There are two flavours of this, and they matter:

- **Classic ESP32 (external USB-UART bridge, e.g. CP2102/CH340).** DTR and RTS
  feed an **auto-reset circuit** on the board that drives **EN** (chip reset) and
  **GPIO0** (boot select). This is the famous two-transistor circuit that lets
  `esptool` reset a board into the bootloader without you touching a button.
- **Native-USB chips (C3 / S3 / C6 / H2).** There's no bridge; the chip's own
  USB-Serial/JTAG peripheral interprets the control lines directly:
  **DTR → GPIO9** (DTR asserted ⇒ enter download mode) and
  **RTS → CHIP_EN** (RTS asserted ⇒ reset).

The classic boards generally won't surprise you. The native-USB boards will,
and here's why.

## The trap: Linux asserts DTR *and* RTS the instant you open the port

When any Linux program opens a USB CDC-ACM serial device, the kernel's
`cdc_acm` driver asserts **both DTR and RTS** as part of opening it. You did not
ask for that; it happens before your code sends a single byte.

On a classic board that's harmless noise. On a native-USB chip it is a loaded
gun: DTR asserted means "enter download mode," so the act of *opening the serial
port* can drop a C3/S3/C6/H2 straight into the bootloader. From your side it
looks like the board "hung" — it's silent, it's not running your firmware — when
in fact it dutifully did what the control lines told it to. **This is the whole
explanation for "my C3 is stuck in download mode."** Nothing crashed; a serial
open asserted DTR.

```
program opens /dev/ttyACM0
        │
   cdc_acm asserts DTR + RTS   ← automatic, unavoidable at open
        │
   native-USB chip sees DTR asserted ⇒ enters download mode
        │
   firmware never runs ⇒ "silent / hung" board
```

## What the workbench does about it

The workbench can't stop `cdc_acm` from doing that, so it works around it in two
ways — one it handles for you, one it requires *you* (or your flashing tool) to
honour.

**It waits before opening.** When a native-USB device first appears, the portal
holds off roughly **two seconds** before opening it, giving the chip time to
finish its own power-on boot so a momentary line state at open doesn't strand it.

**You must pre-set the control lines low — that's what `?ign_set_control` is
for.** An RFC2217 client can tell the proxy "set DTR=False and RTS=False
*before* opening the underlying device," which neutralizes the cdc_acm assertion.
The way you ask for that behaviour is the URL suffix **`?ign_set_control`**.

> **Always connect with `rfc2217://pi4b.local:4001?ign_set_control`. Never
> use a bare `rfc2217://…` URL.** A bare URL lets the default control-line
> assertion through and is exactly how you push a native-USB board into download
> mode by accident.

## Core reset vs. system reset (why a stuck C3 needs a *specific* reset)

So a board is sitting in download mode. Why can't you just toggle a line to kick
it out? Because of *which* reset you issue:

- A **core reset** (the kind you get by pulsing RTS / CHIP_EN) restarts the CPU
  but does **not** re-sample the GPIO9 boot strap. The chip comes back up and
  reads "still in download mode," because the download-mode latch hasn't been
  re-evaluated.
- A **system / watchdog reset** *does* re-sample the GPIO9 strap. Only this kind
  of reset gives the chip a chance to read "GPIO9 is no longer asserting download
  mode" and **leave** the bootloader to run your firmware.

This is precisely why `POST /api/serial/reset` exists and why it's the thing that
*un-sticks* a download-mode board: it performs the right kind of reset. Toggling
a line by hand from a terminal generally won't, which is why the recovery path
runs through the API. The hands-on procedure is in
[Recover a stuck or flapping device](../how-to-guides/recover-a-stuck-or-flapping-device.md).

## What `/api/serial/reset` actually does

Conceptually, for a normal (non-debug) slot, the reset endpoint orchestrates the
serial port carefully so it can both reset the board *and* hand you the boot
output:

1. **Stops the slot's RFC2217 proxy** so nothing else holds the port.
2. **Opens the device with DTR and RTS de-asserted** (the safe state — no
   accidental download mode).
3. **Pulses the reset sequence** to trigger a clean restart.
4. **Captures the boot output** so the response can return the first boot lines.
5. **Restarts the proxy**, returning the slot to normal service.

There's an important special case: **if a JTAG/debug session is active on the
slot, reset doesn't touch USB at all** — it issues a JTAG `reset run` through
OpenOCD instead. Because that path doesn't re-enumerate USB, it's gentle enough
to interrupt and recover even a tight boot loop, which a USB-level reset would
struggle to catch.

## Why flashing prefers `/api/flash`: the dwc_otg hazard and the timing window

Two more rules fall out of the hardware once you look closely.

**The portal never opens a serial device directly.** The workbench enforces a
strict rule: **only the per-slot RFC2217 proxy ever holds the serial port.**
Everything else — including `esptool` — is a *client* of that proxy, never a
second opener of the raw device. Two processes opening the same USB serial device
is a classic way to destabilise the USB stack — on the older `dwc_otg` controller
it could crash the driver outright and take the whole bus down; the single-owner
rule makes that impossible and stays sound on the Pi 4B's xHCI USB. (It's also the
real reason the proxies are separate processes, raised in the
[Architecture overview](architecture-overview.md).)

**Flashing on the Pi sidesteps a timing problem.** Driving the auto-reset
circuit into the bootloader is a *timing-sensitive* dance of DTR/RTS edges.
Done over RFC2217 from a remote laptop, every line change is a network round-trip
(the RFC2217 `SET_CONTROL` exchange), and that latency makes it genuinely hard to
hit the auto-reset window reliably. **`POST /api/flash` runs `esptool` *on the
Pi*,** right next to the proxy, so the timing is local and dependable. That's why
it's the preferred flashing path. When you *do* flash directly over RFC2217 from
a laptop, the workaround is to tell esptool not to fight the reset timing —
`--after no-reset` — and then issue a clean `POST /api/serial/reset` afterwards
to bring the board up.

## What this means for you

- **Always use `?ign_set_control`.** Treat a bare `rfc2217://` URL as a bug. The
  suffix is the difference between "opens cleanly" and "drops my C3 into download
  mode."
- **Prefer `POST /api/flash`.** Local-to-the-Pi timing and the no-double-open
  rule make it the reliable path; flashing over RFC2217 works but needs
  `--after no-reset` plus a follow-up reset.
- **A silent native-USB board is almost certainly in download mode, not dead.**
  Don't start unplugging things — `POST /api/serial/reset` issues the
  system-level reset that re-samples the strap and lets it boot.

## Related

- [Flash firmware](../how-to-guides/flash-firmware.md) — the worked flashing procedures, including `/api/flash`.
- [Recover a stuck or flapping device](../how-to-guides/recover-a-stuck-or-flapping-device.md) — the runbook for un-sticking a board.
- [Control device GPIO](../how-to-guides/control-device-gpio.md) — driving EN/BOOT by wire instead of by control line.
- [Slot states and network ports](../reference/slot-states-and-network-ports.md) — `download_mode` and the rest of the state machine.
