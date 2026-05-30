---
type: explanation
domain: complicated
audience: operator
stability: structural
authority:
  provenance: institutional
  verifiability: auditable
  evidence: strong
  currency: dated
epistemic-layer: framework
---

# Slot identity model

The single most important idea in the workbench is also the least obvious one: a
**slot is a physical USB connector, not a device.** `SLOT1` is "the board plugged
into *that* jack," and it stays `SLOT1` no matter which board is in it, what that
board's serial number is, or what `/dev/tty…` name Linux happens to hand out
this boot. Understanding why the workbench works this way — and how it pulls it
off — is what lets you trust the mapping and stop second-guessing it when devices
move around.

## The problem it solves

If you've ever automated against `/dev/ttyACM0`, you know the pain: those names
are assigned in *enumeration order*, so they shuffle on every replug and every
reboot. The board that was `ttyACM0` this morning is `ttyACM1` after you
unplugged its neighbour. You can't pin to the name.

The obvious fix — pin to the *device* instead, by serial number or VID/PID —
solves the wrong problem. Those identify a **chip**, not a **position.** Swap in
a different board and your "SLOT1" mapping evaporates; plug in two identical
dev boards and they're indistinguishable. What an operator actually wants is
positional: *the board in the top-left jack* should always be `SLOT1`, always on
the same serial/GDB/telnet ports, whatever board that happens to be today.

So the workbench identifies the **jack**, not the board or its kernel name.

## The mechanism: ID_PATH → usb_prefix → longest-prefix match

When a device appears, udev hands the portal a `slot_key`: the device's
**`ID_PATH`** (a topological description of where on the USB tree the device
sits), falling back to the raw `DEVPATH` if `ID_PATH` is missing. The portal
reduces that to a stable **`usb_prefix`** — the substring that names the physical
hub port the device hangs off. Two boards in the same two jacks always produce
the same two prefixes, boot after boot, because the prefix describes *topology*,
not *order of arrival*.

Matching an event to a slot is **longest-prefix-wins.** That rule matters more
than it looks, because USB topologies nest. Consider a sub-hub plugged into a
hub:

```
0:1.1        ← parent hub port  (could be one slot)
0:1.1.4      ← a port on a sub-hub behind it  (a different slot)
```

A device on `0:1.1.4` matches *both* `0:1.1` and `0:1.1.4` as prefixes. Longest
wins, so it lands on the more specific `0:1.1.4` slot — exactly right. Without
longest-prefix matching, a board behind a sub-hub, or the second port of a
dual-USB board, would get mis-attributed to its parent. This is what makes
hubs-behind-hubs and dual-interface boards work.

## What this produces, that you actually feel

The payoff of all that plumbing is a set of guarantees you can lean on:

- **The same jack → the same `SLOT` label → the same ports.** Move a board to a
  different jack and it gets that jack's identity (and ports); leave it put and
  nothing about its addressing ever changes.
- **A board surviving a USB re-enumeration mid-flash stays on its slot.**
  Flashing tools sometimes cause the device to drop and re-appear on the bus. The
  kernel name may change; the `usb_prefix` doesn't, so the slot identity holds
  through it.
- **A dual-USB board is *one* slot.** Boards like many ESP32-S3s expose two USB
  devnodes (a serial one and a JTAG one). They map to a single slot, and that
  slot stays `present` until **both** devnodes are gone. Unplug one and the slot
  is still present — by design.
- **An unrecognized connector still works, dynamically.** Plug a board into a
  jack the portal has no configured slot for and it gets a dynamic **`AUTO-N`**
  slot rather than being ignored. Handy, but `AUTO-N` numbering is assigned as
  devices appear, so it is *not* a stable label to automate against — pin it if
  you care.

## Pinning a jack on purpose

You can make a jack's label and ports deterministic by telling the portal which
`usb_prefix` belongs to which slot. First find a connector's prefix by plugging a
board into it and asking udev:

```bash
udevadm info -q property -n /dev/ttyACM0 | grep ID_PATH
```

Then record that prefix against the slot label (and any fixed port numbers) in
`workbench.json`. After that the jack is nailed down: same label, same ports,
every boot, regardless of enumeration order. The schema and exact fields are in
[Configuration files](../reference/configuration-files.md).

## What this means for you

- **Label your physical jacks once and trust the mapping.** Write `SLOT1`,
  `SLOT2`… on the hub. After that, "the board in jack 1" and `SLOT1` are the same
  thing, permanently.
- **Never automate against `/dev` names.** They are an implementation detail that
  changes under you. Address slots by label through the API; the workbench has
  already done the work of mapping names to positions.
- **A slot showing `present` with "no device" is usually a half-unplugged
  dual-USB board.** Before you worry, check whether a *second* devnode is still
  enumerated — the slot stays present until both are gone.

## Related

- [Configuration files](../reference/configuration-files.md) — pinning labels and ports by `usb_prefix`.
- [Slot states and network ports](../reference/slot-states-and-network-ports.md) — what each slot state means and which port is which.
- [Hardware and wiring](../reference/hardware-and-wiring.md) — the hub and the jacks the slots map to.
- [Tutorial 1 — Build and install the workbench](../tutorials/01-build-and-install-the-workbench.md) — where you first discover and label your slots.
