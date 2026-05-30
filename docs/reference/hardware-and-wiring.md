---
type: reference
domain: complicated
audience: operator
stability: foundational
authority:
  provenance: institutional
  verifiability: auditable
  evidence: strong
  currency: dated
epistemic-layer: practice
---

# Hardware and wiring reference

What you need to build a workbench, and exactly how the optional pins are wired.
The only mandatory hardware is the Pi 4B (in its Argon One M.2 case) and the
boards you want to test; everything else (GPIO control, signal generator, JTAG
probe) is opt-in.

## Bill of materials

| Item | Required? | Notes |
|---|---|---|
| Raspberry Pi 4 Model B | **Yes** | The reference (and only supported) build. Built-in Gigabit Ethernet, dual-band WiFi, Bluetooth, and 4 USB ports. |
| Argon One M.2 case + M.2 **SATA** SSD | **Yes** | Houses the Pi and boots Raspberry Pi OS Lite (64-bit). The SATA board bridges to the Pi over one USB3 port — see [The Argon One M.2 case](#the-argon-one-m2-case). |
| USB-C power supply (official 3A) | **Yes** | The case has its own power button and USB-C input. A Pi driving several boards (and maybe a hub) draws real current — don't under-power it. |
| Powered USB hub | Optional | The Pi 4B leaves ~3 free USB ports after the SSD. Add a **powered** hub only if you need more slots, or to offload current when driving several boards. A 7-port hub helps if you test dual-USB boards. |
| ESP32 boards (the DUTs) | **Yes** | One per slot. See [supported chips](#supported-esp32-targets). |
| USB **data** cables | **Yes** | Charge-only cables are the #1 cause of "device not detected." |
| Jumper wires | Optional | For GPIO reset/boot control. |
| Si5351A breakout | Optional | Signal generator (preferred RF source). |
| PE4302 breakout | Optional | Step attenuator for the signal generator. |
| ESP-Prog (or FT2232H/J-Link/Tigard) | Optional | JTAG debug for classic ESP32 / ESP32-S2, which lack native USB-JTAG. |

## The Argon One M.2 case

The reference build lives in an **Argon One M.2** case, which adds three things
worth knowing about.

- **The SSD rides on a USB3 port — and excludes itself from slots.** The case's
  M.2 SATA board bridges to the Pi through one of its two USB3 ports. That port
  carries a *storage* device, not a serial one, so the portal's slot
  auto-detection skips it automatically: the SSD never shows up as a slot and
  needs no configuration. You're left with ~3 USB ports for ESP32 boards (add a
  powered hub for more).
- **GPIO is under the magnetic cover.** The 40-pin header sits beneath the case's
  magnetic top cover. Lift it off to wire the optional reset/boot pins, the
  signal generator, or a JTAG probe; the pinout is unchanged from a bare Pi.
- **Fan and power button are optional.** They need Argon's daemon, which is *not*
  required for the workbench to run. On headless Raspberry Pi OS you can install
  it with `curl https://download.argon40.com/argon1.sh | bash` and tune the fan
  curve with `argonone-config`. It talks to the case over I²C at address `0x1a`
  — distinct from the Si5351's `0x60`, so it coexists with the signal generator.
  (Check [argon40.com](https://argon40.com) for the current installer.)

## Slots on the Pi 4B

A **slot** is one usable USB port, auto-detected at boot.

The Pi 4B exposes its four USB ports through a built-in hub (the VL805), which is
what the portal enumerates. The Argon SSD claims one USB3 port (and is
auto-excluded, above), so a stock build comes up with roughly **3 slots** — one
USB3 plus two USB2. The exact number is whatever the portal logs at boot and
reports in `/api/devices`; don't assume a fixed count. Need more? Add a powered
USB hub and its ports become additional slots.

If the detected count looks wrong (a port that should be a slot isn't, or vice
versa), see [Maintain and update the workbench → fix a wrong slot count](../how-to-guides/maintain-and-update-the-workbench.md#fix-a-wrong-slot-count-phantom-port-filter).

## Network roles — eth0 vs wlan0

The two interfaces never overlap. This separation is what lets the workbench be
both a network instrument and a thing you reach over the network.

- **eth0 (wired):** all management + serial traffic — the `:8080` API and the
  RFC2217 serial ports. On the Pi 4B this is the built-in Gigabit Ethernet.
- **wlan0 (onboard WiFi):** reserved as the **WiFi test instrument** (SoftAP, station, scan).
- **hci0 (onboard Bluetooth):** the BLE proxy radio.

> The one exception: in **serial-interface mode**, wlan0 stops being an instrument
> and instead joins a WiFi network to provide LAN connectivity. The Pi 4B always
> has built-in Ethernet, so you rarely need this — it's there for setups where
> wlan0 must be the uplink. See [Architecture overview](../explanation/architecture-overview.md#operating-modes).

## Supported ESP32 targets

| Family | USB to the Pi enumerates as | Native USB-JTAG debug? |
|---|---|---|
| ESP32 (classic) | `/dev/ttyUSB*` (CP2102/CH340 bridge) | No — needs an ESP-Prog |
| ESP32-S2 | `/dev/ttyUSB*` | No — needs an ESP-Prog |
| ESP32-C3 | `/dev/ttyACM*` (native USB-Serial/JTAG) | Yes |
| ESP32-S3 | `/dev/ttyACM*` (often two USB ports) | Yes |
| ESP32-C6 | `/dev/ttyACM*` | Yes |
| ESP32-H2 | `/dev/ttyACM*` | Yes |
| Arduino / generic USB serial | `/dev/ttyUSB*` or `/dev/ttyACM*` | n/a |

> **Fake S3 warning:** some "S3" boards put a CH340 USB hub on board. They
> enumerate under VID `1a86` (QinHeng) instead of `303a` (Espressif) and do **not**
> support USB-JTAG.

### JTAG TAP IDs (for native USB-JTAG chips)

| Chip | TAP ID | OpenOCD config |
|---|---|---|
| ESP32-C3 | `0x00005c25` | `board/esp32c3-builtin.cfg` |
| ESP32-C6 | `0x0000dc25` | `board/esp32c6-builtin.cfg` |
| ESP32-H2 | `0x00010c25` | `board/esp32h2-builtin.cfg` |
| ESP32-S3 | `0x120034e5` | `board/esp32s3-builtin.cfg` |

## DUT reset/boot wiring (optional)

Wire these from the Pi's 40-pin header to the board under test if you want
scripted reset, forced download mode, and automatic crash recovery. Without them
you still get serial and (for native-USB chips) JTAG; you just lose hardware
reset/boot control and lose GPIO-assisted flap recovery.

| Pi GPIO (BCM) | Header pin | DUT pin | Function |
|---|---|---|---|
| **GPIO17** | 11 | EN / RST | Hardware reset — **active LOW** |
| **GPIO18** | 12 | GPIO0 (ESP32) / GPIO9 (C3) | Boot-mode select — **active LOW = download mode** |
| GPIO27 | 13 | — | Spare |
| GPIO22 | 15 | — | Spare |

Set these pin numbers in [`workbench.json`](configuration-files.md) as `gpio_en`
and `gpio_boot`. **Pi GPIO is 3.3 V** — wire only to 3.3 V boards.

> **Dual-USB boards usually need no reset wiring.** Their onboard auto-download
> circuit handles reset/boot via DTR/RTS on the JTAG USB port.

### Why "release with `z`" matters here

When you finish driving GPIO17/18, release them to `"z"` (input + pull-up), not
to a driven level. The internal pull-up holds these active-low straps
**de-asserted** (high), so the board boots normally. A pin left driven LOW pins
the board in reset or download mode. The auto-recovery code does exactly this:
it releases BOOT to `"z"` after rescuing a slot.

## Signal generator wiring (optional)

The signal generator picks the best available backend automatically: **Si5351**
(I²C, exact frequencies) if present, otherwise **GPCLK** (the Pi's own hardware
clock, integer-divider frequencies, always available). The **PE4302** adds
0–31.5 dB of switchable attenuation.

### Si5351A (preferred RF source)

| Si5351 pin | Pi pin | BCM |
|---|---|---|
| SDA | pin 3 | GPIO2 (I2C1 SDA) |
| SCL | pin 5 | GPIO3 (I2C1 SCL) |
| VCC | pin 1 | 3.3 V |
| GND | pin 9 | GND |

Default I²C address `0x60` (96). Range ≈ 8 kHz – 160 MHz, channels CLK0–CLK2.
You must enable I²C on the Pi (the installer does this with `raspi-config nonint do_i2c 0`).

### PE4302 step attenuator (3-wire serial mode)

| PE4302 pin | Pi pin | BCM |
|---|---|---|
| DATA | pin 33 | GPIO13 |
| CLK | pin 32 | GPIO12 |
| LE | pin 31 | GPIO6 |
| VCC | — | 3.3 V / 5 V |

Board jumpers for serial mode: **close J4, open J5/J6/J7.** 0–31.5 dB in 0.5 dB steps.

### GPCLK fallback (no extra hardware)

Uses the Pi's hardware clock on **GPCLK1 = GPIO5 (pin 29)** or **GPCLK2 = GPIO6
(pin 31)**. Range ≈ 122 kHz – 250 MHz at integer-divider steps off the 500 MHz PLL.

### Pin conflicts you must respect

⚠️ These overlaps cause silent misbehaviour if ignored:

- **GPCLK pins 5/6 are also general GPIO control pins.** Don't drive the same pin
  via `/api/gpio/set` and `/api/siggen/*` at once.
- **PE4302 LE (GPIO6) is the same pin as GPCLK2.** If you run GPCLK on GPIO6, the
  attenuator's latch is unavailable. To use carrier + live attenuation together,
  use **Si5351** (or run GPCLK on **GPIO5**).
- This is why `/api/gpio/set` only allows pins **16–27**: 2/3 (I²C), 5/6 (GPCLK),
  12/13 (PE4302) are reserved for the instruments.

## JTAG debug probe wiring (classic ESP32 / S2)

Classic ESP32 and ESP32-S2 have no native USB-JTAG, so they need an external
probe (ESP-Prog ≈ \$15, FT2232H-based; or FT232H, J-Link, Tigard). The probe
takes one USB hub port and is declared under `debug_probes[]` in
[`workbench.json`](configuration-files.md).

| Chip | TCK | TDI | TDO | TMS |
|---|---|---|---|---|
| ESP32 (classic) | GPIO13 | GPIO12 | GPIO15 | GPIO14 |
| ESP32-C3/C6/H2 | 4 | 5 | 6 | 7 |
| ESP32-S2/S3 | 39 | 40 | 41 | 42 |

> **Classic-ESP32 GPIO12 strapping hazard:** TDI lands on GPIO12, which is the
> flash-voltage strap. If it's HIGH at power-up the chip selects 1.8 V flash and a
> 3.3 V board crashes. Mitigate by burning the `VDD_SDIO` eFuse to 3.3 V, or keep
> TDI low at boot. See [Debug with GDB](../how-to-guides/debug-with-gdb.md).

## Related

- [Configuration files](configuration-files.md) — where these pin numbers live.
- [Slot states and network ports](slot-states-and-network-ports.md) — the port map.
- [Tutorial 1 — Build and install](../tutorials/01-build-and-install-the-workbench.md) — puts this together.
