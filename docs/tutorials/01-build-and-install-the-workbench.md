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

# Tutorial 1 — Build and install the workbench

By the end of this tutorial you'll have a Raspberry Pi answering at
`http://pi4b.local:8080`, with every ESP32 you plug into it showing up as a
slot you can drive over HTTP. We'll go from a pile of parts to a working
instrument in a few steps.

This is the **builder track** — for whoever provisions the rig. Once it's alive,
hand the operators [Tutorial 2 — Your first flash and test](02-your-first-flash-and-test.md).

> **Time:** ~20 minutes, most of it unattended (package installs).
> **You need:** a Pi 4B in an Argon One M.2 case already running Raspberry Pi OS
> Lite (64-bit) headless from its SSD, the parts below, an SSH client on your
> desktop, and your spare unmanaged switch.
>
> **Prerequisite — OS already imaged.** These docs assume Raspberry Pi OS Lite
> (64-bit) is already on the SSD and the Pi boots headless with **SSH enabled**
> and hostname **`pi4b`**. If it isn't, image it first with [Raspberry Pi
> Imager](https://www.raspberrypi.com/software/) (set the hostname to
> `pi4b`, enable SSH, set your locale/WiFi country), boot once, then come
> back here.

---

## What you're building

```
        your desktop                      the workbench
   ┌──────────────────┐   wired LAN   ┌────────────────────────────┐
   │  browser / curl  │◀────eth0─────▶│  Raspberry Pi 4B           │
   │  esptool / pio   │  :8080 API    │   rfc2217-portal service   │
   └──────────────────┘  :4001 serial │                            │
                                       │   USB ports ┬── SLOT1 ESP32│
        WiFi test traffic             │             ├── SLOT2 ESP32│
   (devices) ◀───────wlan0───────────▶│             └── SLOT3 ESP32│
                                       │   wlan0 = WiFi instrument  │
                                       └────────────────────────────┘
```

The Pi is reached over **wired Ethernet (eth0)**. Its **WiFi (wlan0)** is held
back as a test instrument, not a network uplink. (Full picture:
[Architecture overview](../explanation/architecture-overview.md).)

---

## Step 1 — Gather the parts

Minimum to get a working rig:

- The **Raspberry Pi 4B** in its **Argon One M.2** case, booting Raspberry Pi OS
  Lite (64-bit) from the SSD (see the prerequisite above).
- Its **USB-C power supply**.
- One or more ESP32 boards and **USB data cables** (not charge-only).
- Your spare **unmanaged switch** and an Ethernet cable, to share the room's
  single wall jack with your desktop (Step 3).

No USB hub or USB-Ethernet adapter is needed: the Pi 4B has built-in Gigabit
Ethernet and ~3 free USB ports after the SSD. Add a powered hub later only if you
outgrow those ports.

Optional add-ons you can wire later: jumper wires for [GPIO reset/boot
control](../reference/hardware-and-wiring.md#dut-resetboot-wiring-optional), an
[Si5351 + PE4302 signal generator](../reference/hardware-and-wiring.md#signal-generator-wiring-optional),
and an [ESP-Prog](../reference/hardware-and-wiring.md#jtag-debug-probe-wiring-classic-esp32--s2)
for debugging classic ESP32s.

The complete list is in [Hardware and wiring](../reference/hardware-and-wiring.md#bill-of-materials).

## Step 2 — Assemble the hardware

The Pi is already built into the Argon One M.2 case and booting from the SSD, so
there's little to do here.

1. **Don't plug in the ESP32 boards yet** — we'll add them after the software is
   up, so you can watch slots appear.
2. *(Optional, do it now if you want it):* lift the case's magnetic cover and wire
   GPIO reset/boot and the signal generator per [Hardware and wiring](../reference/hardware-and-wiring.md).
   You can also add these after first boot.

> **Power matters.** A Pi feeding several boards needs an adequate supply, and if
> you add a USB hub it should be **externally powered**. Under-powered USB is the
> most common cause of flaky enumeration.

## Step 3 — Connect it to your network

The workbench is reached over **wired Ethernet**, and your desktop is the machine
you'll drive it from. You have one wall jack in the room and your desktop is
already on it, so put the spare **unmanaged ("dumb") switch** between them.

```
   Internet ──WAN──▶ ┌──────────────────────────┐
                     │  UniFi Dream Machine Pro │  router · gateway · DHCP · DNS
                     │     (your router)        │
                     └───────────┬──────────────┘
                                 │  one cable run to your room
                                 ▼
                         ┌───────────────┐
                         │  wall socket  │  the single RJ45 jack in the room
                         └───────┬───────┘
                                 ▼
                     ┌───────────────────────────┐
                     │ unmanaged ("dumb") switch  │  fans one jack out to many,
                     └───┬───────────────────┬────┘  all on the same LAN segment
                         ▼                   ▼
              ┌──────────────────┐  ┌──────────────────────────────┐
              │ desktop computer │  │ Raspberry Pi 4B — eth0       │
              │ run tests here   │  │ Argon One M.2 · the workbench│
              └──────────────────┘  │ http://pi4b.local:8080  │
                                     └──────────────────────────────┘
```

What each box does:

| Device | Role in this setup |
|---|---|
| UniFi Dream Machine Pro (*your router*) | Gateway + **DHCP server** — hands the Pi an IP automatically. No per-device setup for the basic case. |
| Single wall socket | The one RJ45 jack in the room; one cable back to the router. |
| Unmanaged ("dumb") switch | Splits that one jack into several. It's transparent layer-2, so every device on it shares **one broadcast domain** — which is what lets `pi4b.local` (mDNS) resolve. |
| Desktop computer | Your **control machine** (browser, `curl`, `esptool`/`pio`). Already on the socket; it moves onto the switch. |
| Raspberry Pi 4B — `eth0` | The workbench. Its built-in Gigabit Ethernet plugs into the switch; **`wlan0` stays free** as the WiFi test instrument. |

Wire it up:

1. Unplug your desktop's Ethernet from the wall socket.
2. Run a cable from the **wall socket** to one port of the **dumb switch**.
3. Plug your **desktop** into a second switch port, and the **Pi's `eth0`** into a
   third.
4. Power the Pi. It pulls a DHCP lease from your router automatically — no static
   config needed.

**Why this works:** because the desktop and the Pi hang off the same dumb switch,
they're on the same subnet and the same layer-2 segment. That's all mDNS needs, so
`pi4b.local` resolves from your desktop with zero router configuration.

> **If you later split them across VLANs** (say, the Pi on an IoT VLAN on the UDM
> Pro and your desktop on the main LAN), `pi4b.local` and the UDP discovery
> beacon stop crossing on their own — you'd need the router's mDNS reflector and
> an inter-VLAN allow rule. The single-switch setup above avoids all of that.

Give it a minute, then from your desktop:

```bash
ssh pi4b@pi4b.local
```

If that connects, the network is good and you can set up the software.

## Step 4 — Confirm the hostname

`install.sh` does **not** set the hostname. If you set it when you imaged the SSD,
you're done. If not, set it now so `pi4b.local` works:

```bash
sudo hostnamectl set-hostname pi4b
# log out and back in; confirm:
hostname    # → pi4b
```

> **Why `pi4b` and not `serial1`?** Older notes in `pi/README.md` say
> `Serial1`. The rest of the system — these docs, the skills, the discovery
> beacon — all expect **`pi4b.local`**. Use `pi4b`.

## Step 5 — Install the workbench software

Clone the project onto the Pi and run the installer:

```bash
git clone https://github.com/SensorsIot/Universal-Embedded-Workbench.git
cd Universal-Embedded-Workbench/pi
sudo bash install.sh
```

`install.sh` does all of this for you:

- Installs system packages: `python3-serial`, `python3-libgpiod`, `hostapd`,
  `dnsmasq-base`, `mosquitto`, `iptables`, `bluetooth`/`bluez`, and pip-installs
  `esptool`, `bleak`, `smbus2`.
- Downloads and installs Espressif's **OpenOCD** build to `/usr/local/bin/openocd-esp32`.
- Enables I²C (for the signal generator).
- **Disables and masks** `hostapd`, `dnsmasq`, and `mosquitto` — the portal drives
  these dynamically, so they must not run on their own.
- Copies the portal and all controllers into `/usr/local/bin/`.
- Installs the `rfc2217-portal` systemd service and the udev hotplug rules.
- **Enables and starts the service.**

When it finishes it prints:

```
Portal running at: http://<your-pi-ip>:8080
```

> It prints the **IP**, not `pi4b.local`, because the installer doesn't know
> your hostname. Both work.

## Step 6 — Verify it's alive, then add boards

On the Pi:

```bash
curl -s http://localhost:8080/api/info | jq .
```

You should get a JSON object with `host_ip`, `hostname`, and slot counts. (If you
get connection-refused, the port is **8080**, not 5000 — and check
`sudo systemctl status rfc2217-portal`.)

From your **desktop**, confirm mDNS works:

```bash
curl -s http://pi4b.local:8080/api/info | jq .
```

Now **plug in your ESP32 boards**, one per hub port, and watch them appear:

```bash
curl -s http://pi4b.local:8080/api/devices | jq '.slots[] | {label, present, state, detected_chip, url}'
```

Each occupied port becomes a `SLOTn` with `present: true`, an auto-detected
`detected_chip`, and a serial `url` like `rfc2217://pi4b.local:4001`.

🎉 **Your workbench is live.** Open `http://pi4b.local:8080` in a browser to
see the dashboard.

---

## (Optional) Pin and label your slots

Slots auto-detect every boot, so this is only worth doing if you want stable,
human-friendly labels tied to specific physical jacks (e.g. "the top-left port is
always `SLOT1`"). On the Pi:

```bash
sudo rfc2217-learn-slots
```

It prints a ready-to-paste `workbench.json`. Save it and restart:

```bash
sudo cp my-slots.json /etc/rfc2217/workbench.json
sudo systemctl restart rfc2217-portal
```

Details and the full schema: [Configuration files](../reference/configuration-files.md)
and [Slot identity model](../explanation/slot-identity-model.md).

## Troubleshooting first boot

| Symptom | Fix |
|---|---|
| `pi4b.local` won't resolve | Use the IP the installer printed; check the Pi is on wired Ethernet; mDNS can be flaky on some networks. |
| `curl :8080` refused | `sudo systemctl status rfc2217-portal`; logs: `sudo journalctl -u rfc2217-portal -f`. |
| A board doesn't appear as a slot | Use a **data** cable; if you added a USB hub, make sure it's powered; check `lsusb` on the Pi. |
| Slot count looks wrong | The SSD's USB3 port is auto-excluded; a hub can also advertise phantom ports — see [fix a wrong slot count](../how-to-guides/maintain-and-update-the-workbench.md#fix-a-wrong-slot-count-phantom-port-filter). |

## Next steps

- **Use it:** [Tutorial 2 — Your first flash and test](02-your-first-flash-and-test.md).
- **Keep it running:** [Maintain and update the workbench](../how-to-guides/maintain-and-update-the-workbench.md).
