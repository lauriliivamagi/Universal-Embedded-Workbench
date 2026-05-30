---
type: explanation
domain: complex
audience: operator
stability: structural
authority:
  provenance: institutional
  verifiability: auditable
  evidence: moderate
  currency: dated
epistemic-layer: framework
---

# Crash recovery and flapping

A board with bad firmware doesn't fail quietly. It can fail in a way that
threatens the *whole workbench* — rebooting so fast that it hammers the USB bus
hundreds of times a minute. This page explains what that "flapping" is, why a
low-RAM Pi has to actively defend itself against it, and how the workbench
turns a self-inflicted brick back into a board you can simply re-flash. The aim
is understanding, not procedure; when you need the steps, the runbook is
[Recover a stuck or flapping device](../how-to-guides/recover-a-stuck-or-flapping-device.md).

## What flapping is, and why it's dangerous

When an ESP32 has corrupt or empty flash, or is caught in a boot loop, it can
**connect and disconnect on the USB bus over and over, very fast** — each crash
re-enumerates the device. That's *flapping.*

It's not merely annoying. Every USB add/remove event costs the host kernel work,
and a storm of *hundreds* of disconnects can saturate the USB stack and pin a CPU
core to the point where the Pi becomes unresponsive — **SSH and even the portal
itself can stop answering.** So this isn't a nicety the workbench does for
tidiness; it's **self-defence.** One bad board must not be able to take the
instrument down.

## How the workbench detects it

The portal watches each slot's hotplug events through a sliding window and
reacts on thresholds. Treat these numbers as *the constants that drive the
behaviour you observe*, not as knobs you tune:

| Quantity | Value | What it's for |
|---|---|---|
| Sliding window | **30 seconds** | the span over which events are counted |
| Trip threshold | **~10 events** in the window | crossing it marks the slot `flapping` |
| Cooldown | **~10 seconds** between recovery attempts | keeps recovery from itself becoming a storm |
| Retries | up to **2** | how many times a no-GPIO slot will try before giving up |

The 10-event threshold is deliberately set *above* the noise floor of normal
operation. A healthy **dual-USB board produces about 2 events per plug** (two
devnodes appearing), so the workbench can tell "an S3 was just plugged in" apart
from "a board is melting down" — the former never approaches 10-in-30s, the
latter blows past it.

## Stopping the storm

Detection alone doesn't help if the board keeps cycling. The workbench halts the
storm at the **kernel level**: it *unbinds* the offending USB device from its
driver by writing the device's sysfs name to
`/sys/bus/usb/drivers/usb/unbind`. With the device unbound, the kernel stops
trying to enumerate it, and the flood stops.

It also stops *itself* from being re-triggered: **while a slot is recovering, the
portal ignores further hotplug events for that slot.** Otherwise the very acts of
unbinding and rebinding would generate events that look like more flapping and
restart the cycle. The slot is quarantined until recovery finishes.

## The recovery paths

What happens after the storm is stopped depends on how the slot is wired.

**1. A GPIO-wired slot — the good outcome.** If the slot has reset/boot pins
wired (GPIO17→EN, GPIO18→BOOT), the workbench can force a known-good state. After
the cooldown it **holds BOOT LOW, pulses EN, and rebinds the USB device.** The
board comes back up *in the bootloader* instead of re-running the firmware that
was crashing it. The slot settles into a **stable `download_mode` state, with
BOOT still held LOW**, waiting for you to flash known-good firmware and then call
`POST /api/serial/release` to let it boot. This is the happy path: a flapping
board becomes a parked, flashable board with no babysitting.

**2. A no-GPIO slot — best effort.** Without reset/boot wiring the workbench has
no way to force the bootloader; all it can do is **unbind, wait the cooldown,
rebind, and hope the board enumerates cleanly.** It will retry this **up to
twice.** If the board keeps flapping through the retries, the workbench **gives
up gracefully** and leaves the slot in state `flapping` with `last_error` reading
*"needs manual intervention."* That's your cue to flash on the Pi with esptool's
`--before=usb_reset`, which performs the reset the missing GPIO can't.

**3. A slot with an active debug session — halt at the source.** If OpenOCD/JTAG
is attached, the workbench prefers a **`monitor halt`**: it stops the CPU *at the
point it's crashing*, which is both more informative and gentler than a USB
unbind. Only if that isn't viable does it fall back to the unbind/GPIO paths
above.

```
            flapping detected (≥~10 events / 30 s)
                          │
                  unbind USB device  (stop the storm)
                          │
        ┌─────────────────┼──────────────────────────┐
   debug session?     GPIO wired?                 no GPIO?
        │                 │                           │
   monitor halt    hold BOOT LOW, pulse EN,     unbind→wait→rebind,
   (stop at fault)   rebind → download_mode        retry ≤ 2×
                    (stable, awaiting flash)          │
                                              still flapping?
                                                      │
                                          give up: state=flapping,
                                          last_error="needs manual
                                          intervention"
```

While any of this is in progress, **auto-debug (OpenOCD) is suppressed** for the
slot. A board that's flapping or recovering won't *also* have an OpenOCD session
trying to attach to it — the workbench won't let recovery and debug fight over
the same unstable device.

## Why a `flapping` flag sometimes lingers, then clears on its own

You may see a slot still flagged `flapping` after the board has actually settled,
and then watch the flag clear without you doing anything. That's expected, and
it follows directly from the sliding window. The flag is derived from "how many
events fall in the last 30 seconds." Once a transient flap stops, those events
keep **aging out** of the window; the **next `GET /api/devices` poll** recomputes
the count, finds it's dropped below the threshold, and clears the flag. So a
brief flap that has since stabilized resolves itself — the lingering flag is the
window emptying, not a stuck state.

## What this means for you

- **Flapping is usually self-inflicted and usually self-heals.** It almost always
  means *the firmware you just flashed is bad*, and on a GPIO-wired slot it
  resolves on its own into a parked `download_mode` board.
- **Your job is the easy half.** Once a slot reaches `download_mode`, flash
  known-good firmware and then `POST /api/serial/release`. The workbench already
  did the dangerous part (stopping the storm, parking the board).
- **A slot stuck at `flapping` with "needs manual intervention" is the no-GPIO
  give-up.** That's the one case the workbench can't finish for you — flash on
  the Pi with `--before=usb_reset`. The steps are in the
  [recovery runbook](../how-to-guides/recover-a-stuck-or-flapping-device.md).

## Related

- [Recover a stuck or flapping device](../how-to-guides/recover-a-stuck-or-flapping-device.md) — the hands-on runbook.
- [Serial reset and download mode](serial-reset-and-download-mode.md) — why the bootloader-parked state is `download_mode` and how reset behaves.
- [Control device GPIO](../how-to-guides/control-device-gpio.md) — the reset/boot wiring that enables the good recovery path.
- [Slot states and network ports](../reference/slot-states-and-network-ports.md) — the `flapping` / `recovering` / `download_mode` states.
