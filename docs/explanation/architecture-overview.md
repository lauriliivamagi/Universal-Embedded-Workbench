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

# Architecture overview

If you understand one thing about the workbench, make it this: from your laptop
there is exactly **one** thing to talk to. Everything the workbench can do —
flashing, serial, GPIO, WiFi, BLE, debug, signal generation — is an HTTP call to
a single service on a single port. The hardware sprawl behind that port (a hub
full of boards, a WiFi radio, a Bluetooth radio, optional jumper wires) is the
service's problem, not yours. This page explains what that service is, what it
runs, and how the network is carved up — so that when something behaves oddly
you know which box to look in.

## One service, run as root, kept alive by systemd

The whole workbench is a single Python program, `rfc2217-portal`, running on the
Pi. systemd starts it at boot and, crucially, restarts it on failure
(`Restart=on-failure`). It runs **as root** — not for convenience, but because
some of its instruments poke hardware directly through `/dev/mem` (the GPCLK
clock generator and the low-level GPIO primitives need that). Everything you do
as an operator is an HTTP request to this service on **`:8080`**.

```
        your laptop
            │  HTTP
            ▼
  ┌───────────────────────────┐
  │   rfc2217-portal (:8080)   │  ← one Python process, root, systemd-supervised
  │                            │
  │   supervises ↓ subsystems  │
  └───────────────────────────┘
```

Because it is one supervised process, a crash is not a disaster you have to
attend to — systemd brings it straight back. The cost is that *all* state lives
in that one process, so a restart resets in-memory things like ring buffers and
the recovery bookkeeping. That trade-off (self-healing vs. ephemeral state) is
worth keeping in mind when you read [Crash recovery and flapping](crash-recovery-and-flapping.md).

## What the one service supervises

The portal is less a web app than a supervisor that owns a fleet of subsystems
and exposes each as a slice of the API:

| Subsystem | What it is | How you reach it |
|---|---|---|
| Per-slot serial proxies | One `plain_rfc2217_server.py` process **per active slot**, each holding that slot's USB serial device and exposing it as an RFC2217 TCP port | `/api/serial/*`, `rfc2217://…:4001+` |
| WiFi instrument | SoftAP / station / scan / HTTP-relay on `wlan0` | `/api/wifi/*` |
| GPIO control | Drives the Pi's BCM pins via gpiod | `/api/gpio/*` |
| Signal generator | RF source: Si5351 (I²C) or GPCLK fallback, optional PE4302 attenuator | `/api/siggen/*` |
| GDB debug | OpenOCD sessions over JTAG, one per slot | `/api/debug/*`, GDB `3333+`, telnet `4444+` |
| BLE proxy | `bleak` against the `hci0` radio, one connection at a time | `/api/ble/*` |
| UDP log receiver | Collects log lines firmware sends to `5555/udp` | `/api/udplog` |
| OTA firmware repository | Stores `.bin` files and serves them at `/firmware/` | `/api/firmware/*`, `/firmware/<project>/<file>` |
| Test progress + human-interaction panel | Tracks a running test and can **block** for an operator to click Done/Cancel | `/api/test/*`, `/api/human*` |
| Discovery beacon | Answers `DISCOVER` probes on `5888/udp` so clients can find the workbench without knowing its address | `5888/udp` |

The point of the table is not to memorize it — the [REST API reference](../reference/rest-api.md)
is the exhaustive list — but to see the shape: each capability is an independent
subsystem, and the portal is the thing that starts, watches, and routes to them.

A consequence worth internalizing: the per-slot serial proxies are **separate
processes**. The portal itself never opens a serial device. That sounds like a
detail but it is load-bearing for stability on the Pi — the reasons are in
[Serial reset and download mode](serial-reset-and-download-mode.md).

## The eth0 / wlan0 / hci0 split

The workbench is simultaneously *a thing on the network* and *a network test
instrument*, which would be a contradiction if both used the same radio. They
don't. Three interfaces, three non-overlapping jobs:

- **eth0 (wired)** — **all** management and serial traffic: the `:8080` API and
  every RFC2217 serial port. This is how you and your test scripts talk to the
  workbench.
- **wlan0 (onboard WiFi)** — the **WiFi test instrument** (SoftAP, station,
  scan, HTTP relay). It is *equipment under your command*, not your link to the
  Pi.
- **hci0 (onboard Bluetooth)** — the **BLE proxy** radio.

They never overlap, and that is deliberate: it means a WiFi test (taking `wlan0`
down, joining a flaky AP, starting a SoftAP) can never knock out your control
channel, because your control channel is on the wire. The full role breakdown is
in [Hardware and wiring → Network roles](../reference/hardware-and-wiring.md#network-roles--eth0-vs-wlan0).

## Operating modes

The workbench runs in one of two **operating modes**, and the mode decides what
`wlan0` is *for*:

| Mode | `wlan0` is… | WiFi-instrument endpoints | Use it when |
|---|---|---|---|
| **wifi-testing** (default) | an **instrument** under your control | enabled | normal operation — you have wired Ethernet and you want to test devices' WiFi |
| **serial-interface** | a **client** that joins a real WiFi network to give the Pi LAN access | **disabled** | the Pi has no Ethernet and `wlan0` is its only way onto the network |

In `serial-interface` mode the WiFi-instrument calls (`scan`, `ap_start`,
`sta_join`, `http`) return an error, because the radio is busy being your uplink
instead of your instrument. Switching modes is a `POST /api/wifi/mode` away, and
the switch is defensive: **if a mode change fails, the workbench auto-reverts to
`wifi-testing`** rather than stranding itself on a half-configured radio. The
how-to is [Run WiFi tests](../how-to-guides/run-wifi-tests.md).

## How hotplug works (at a high level)

You don't tell the workbench when you plug a board in — it finds out. A udev
rule fires on every USB add and remove. Because udev runs its helpers in a
locked-down sandbox with no network access, the rule can't just talk to the
portal directly; it re-launches the notification via `systemd-run` to escape
that sandbox, and *that* process does a `POST /api/hotplug` to the portal. The
portal then maps the raw kernel event to one of your slots.

That last step — turning a `/dev/ttyACM…` add event into "SLOT2" — is the
interesting part, and it's where the workbench's whole identity model lives. It
gets its own page: [Slot identity model](slot-identity-model.md). Here, just
hold the shape: **kernel → udev → systemd-run → `/api/hotplug` → a slot.**

## Security posture

Be clear-eyed about this: the workbench has **no authentication, no
authorization, and no transport security.** Every endpoint on `:8080` is open to
anyone who can reach the port, and CORS is wide open
(`Access-Control-Allow-Origin: *`) so any web page can call it too. This is a
deliberate design choice for a *trusted-LAN lab instrument*, not an oversight —
but it means the threat model is entirely "keep it on a network you trust."

Two endpoints make this especially consequential, because they turn "reach the
port" into "run code / reach the internet":

- **`POST /api/flash`** runs arbitrary `esptool` against operator-uploaded
  binaries on the Pi. Anyone who can hit it can flash anything onto your boards.
- **`POST /api/wifi/http`** relays an arbitrary HTTP request *from the Pi*. It is
  an open proxy onto whatever the Pi's WiFi can see — including a device's
  private AP subnet.

The rule that follows: treat `:8080` as a **trusted-LAN-only control plane.**
Never expose it to the internet, never port-forward it. If you need to reach the
workbench remotely, tunnel in — an **SSH tunnel or a VPN** to the lab network —
so the authentication lives in the tunnel, where the workbench has none of its
own.

## What this means for you

- **You only ever talk to one port.** If automation can't do something, the
  question is almost always "which subsystem owns this, and what does
  `/api/devices` say about it?" — not "which host/port should I use." It's
  always `pi4b.local:8080`.
- **A crash is self-healing.** systemd restarts the portal on failure, so a
  blip recovers on its own; you mostly notice it as reset in-memory state (logs,
  buffers), not as a dead workbench.
- **Keep it off untrusted networks.** There is no lock on the door. The LAN *is*
  the security boundary; for anything beyond the LAN, use a tunnel.

## Related

- [Slot identity model](slot-identity-model.md) — how a USB event becomes a slot.
- [Crash recovery and flapping](crash-recovery-and-flapping.md) — how the portal defends itself.
- [REST API reference](../reference/rest-api.md) — every endpoint each subsystem exposes.
- [Slot states and network ports](../reference/slot-states-and-network-ports.md) — the state machine and full port map.
- [Hardware and wiring](../reference/hardware-and-wiring.md) — the physical rig behind the service.
