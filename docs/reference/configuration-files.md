---
type: reference
domain: complicated
audience: operator
stability: structural
authority:
  provenance: institutional
  verifiability: testable
  evidence: strong
  currency: dated
epistemic-layer: practice
---

# Configuration files

All operator-editable config lives in **`/etc/rfc2217/`** on the Pi. None of it is
required to get going — slots auto-detect and the signal generator reads sensible
defaults. You edit these only to pin slot labels/ports, change instrument wiring,
or declare a JTAG probe. After editing either file:

```bash
sudo systemctl restart rfc2217-portal
```

## `workbench.json` — slots, GPIO, debug probes

**Location:** `/etc/rfc2217/workbench.json` (absent by default → full auto-detect).
**Template in repo:** `pi/config/workbench.json`.

Generate a starting file from the boards currently plugged in:

```bash
ssh pi4b@pi4b.local sudo rfc2217-learn-slots
# paste the output to /etc/rfc2217/workbench.json, then restart the service
```

```json
{
  "gpio_boot": 18,
  "gpio_en": 17,
  "slots": [
    {
      "label": "SLOT1",
      "usb_prefix": "0:1.1",
      "tcp_port": 4001,
      "gdb_port": 3333,
      "openocd_telnet_port": 4444
    },
    {
      "label": "SLOT2",
      "usb_prefix": "0:1.2",
      "tcp_port": 4002,
      "gdb_port": 3334,
      "openocd_telnet_port": 4445
    }
  ],
  "debug_probes": [
    {
      "label": "PROBE1",
      "type": "esp-prog",
      "interface_config": "interface/ftdi/esp_ftdi.cfg",
      "bus_port": "1-1.4:1.0"
    }
  ]
}
```

| Key | Scope | Meaning |
|---|---|---|
| `gpio_boot` | top-level | Pi BCM pin wired to the DUT boot pin (GPIO0 / GPIO9). Default `18`. |
| `gpio_en` | top-level | Pi BCM pin wired to the DUT EN/RST pin. Default `17`. |
| `label` | slot | Slot name (`SLOT1`…). What you pass as `"slot"` in API calls. |
| `usb_prefix` | slot | The stable hub-port identifier that ties this label to a physical connector. |
| `tcp_port` | slot | RFC2217 serial port. Defaults to `4001 + index`. |
| `gdb_port` | slot | OpenOCD GDB port. Defaults to `3333 + index`. |
| `openocd_telnet_port` | slot | OpenOCD telnet port. Defaults to `4444 + index`. |
| `debug_probes[]` | top-level | External JTAG probes (ESP-Prog/FT2232H) for chips without native USB-JTAG. |

> Find a connector's `usb_prefix` by plugging a board in and running
> `udevadm info -q property -n /dev/ttyACM0 | grep ID_PATH` — the workbench
> reduces the `ID_PATH` to the stable prefix for you. See
> [Slot identity model](../explanation/slot-identity-model.md).

## `signalgen.json` — signal generator wiring

**Location:** `/etc/rfc2217/signalgen.json` (installed once if absent; edit only if
your wiring differs). **Template in repo:** `pi/config/signalgen.json`.

```json
{
  "si5351": { "bus": 1, "address": 96, "default_channel": 0 },
  "gpclk":  { "default_pin": 5 },
  "pe4302": { "enabled": true, "data_pin": 13, "clk_pin": 12, "le_pin": 6 }
}
```

| Key | Meaning |
|---|---|
| `si5351.bus` | I²C bus number (1 = I2C1, the default Pi bus). |
| `si5351.address` | I²C address in **decimal** — `96` = `0x60`. |
| `si5351.default_channel` | Default CLK output (0–2). |
| `gpclk.default_pin` | GPCLK pin used by the fallback backend (5 or 6). |
| `pe4302.enabled` | Whether a PE4302 attenuator is wired. |
| `pe4302.data_pin` / `clk_pin` / `le_pin` | BCM pins for the 3-wire interface. Note `le_pin` 6 = GPCLK2 — see the [pin conflicts](hardware-and-wiring.md#pin-conflicts-you-must-respect). |

## Mosquitto broker

**Location:** `/etc/mosquitto/conf.d/test-broker.conf` (installed by `install.sh`).
Listens on `1883/tcp`, `allow_anonymous true`. There is **no portal API** to start
or stop it — devices and `mosquitto_pub`/`mosquitto_sub` connect directly. It is
disabled at the systemd level by default; enable it only if you need MQTT testing.

## Files the installer manages (not for hand-editing)

| Path | What it is |
|---|---|
| `/etc/systemd/system/rfc2217-portal.service` | The portal service unit. |
| `/etc/udev/rules.d/99-rfc2217-hotplug.rules` | Hotplug → `POST /api/hotplug`. |
| `/etc/udev/rules.d/60-openocd.rules` | USB permissions for JTAG (VID `303a`, `0403`). |
| `/usr/local/bin/rfc2217-portal` | The portal itself (installed copy of `portal.py`). |
| `/var/lib/rfc2217/firmware/` | OTA firmware repository served at `/firmware/`. |

To change any of these, edit the source in the repo and re-run the installer —
see [Maintain and update the workbench](../how-to-guides/maintain-and-update-the-workbench.md).
