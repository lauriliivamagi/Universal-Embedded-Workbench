---
type: reference
domain: clear
audience: machine
stability: structural
authority:
  provenance: canonical
  verifiability: executable
  evidence: strong
  currency: dated
epistemic-layer: practice
---

# REST API reference

Every workbench function is an HTTP endpoint on `http://pi4b.local:8080`.
This page is derived from `pi/portal.py` (and the per-subsystem controllers), so
it reflects what the code actually does.

## Conventions

- **Base URL:** `http://pi4b.local:8080` (substitute your `SERIAL_PI` IP if you don't use mDNS).
- **No authentication.** Every endpoint is open on the LAN. CORS is wide open (`Access-Control-Allow-Origin: *`).
- **Response envelope:** most handlers return JSON with an `"ok": true|false` field. Treat `ok: false` as a failure; the message is in `"error"`.
- **Slots are addressed by label** (`"SLOT1"`) in all `/api/serial/*`, `/api/flash`, and `/api/debug/*` calls.
- **Content type:** POST bodies are JSON unless the row says *multipart/form-data*.
- Unknown paths return `404 {"error": "not found"}`.

---

## Slots, devices, proxy

| Method | Path | Purpose | Key request | Key response |
|---|---|---|---|---|
| GET | `/api/devices` | Full state of every slot | — | `slots[]`, `host_ip`, `hostname`. Each slot: `label`, `slot_key`, `tcp_port`, `gdb_port`, `openocd_telnet_port`, `state`, `present`, `running`, `pid`, `devnode`, `devnodes[]`, `url`, `flapping`, `recovering`, `recover_retries`, `has_gpio`, `gpio_boot`, `gpio_en`, `debugging`, `debug_chip`, `debug_gdb_port`, `detected_chip`, `jtag_slot`, `usb_devices[]`, `usb_warning`, `is_probe`, `last_error` |
| GET | `/api/info` | Host summary | — | `host_ip`, `hostname`, `slots_configured`, `slots_running` |
| POST | `/api/start` | Force-start the proxy on a slot | `slot_key`, `devnode` | `ok`, `slot_key`, `running` |
| POST | `/api/stop` | Stop the proxy on a slot | `slot_key` | `ok`, `slot_key`, `running` |
| POST | `/api/hotplug` | **Internal** — udev calls this on add/remove | `action`, `devnode`, `id_path`, `devpath` | `ok`, `slot_key`, `accepted` |

`state` is one of: `absent`, `idle`, `resetting`, `monitoring`, `flapping`,
`recovering`, `download_mode`, `debugging` — see
[Slot states and network ports](slot-states-and-network-ports.md).

---

## Serial: reset, monitor, recovery

| Method | Path | Purpose | Key request | Key response |
|---|---|---|---|---|
| POST | `/api/serial/reset` | Reset the board (JTAG `reset run` if a debug session is active, else a DTR/RTS pulse) | `slot` *(req)*, `lines` (opt) | `ok`, `output[]` (boot lines) |
| POST | `/api/serial/monitor` | Read serial via the running proxy, optionally wait for a substring | `slot` *(req)*, `pattern`, `timeout` (s, default 10) | `ok`, `matched`, `line`, `output[]` |
| GET | `/api/serial/output` | Passive read of the slot's ring buffer (does not disturb the proxy) | `slot` *(req)*, `lines` (default 50), `since` (epoch) | `ok`, `lines[]` (`{ts, text}`) |
| POST | `/api/serial/recover` | Manually trigger USB-flap recovery (unbind/rebind; GPIO download-mode if wired) | `slot` *(req)* | `ok`, `message` |
| POST | `/api/serial/release` | After flashing in `download_mode`: release BOOT to high-Z and pulse EN to reboot | `slot` *(req)* | `ok` |

---

## Flashing and firmware

| Method | Path | Purpose | Key request | Key response |
|---|---|---|---|---|
| POST | `/api/flash` | *multipart/form-data.* Upload binaries and run `esptool write_flash` on the Pi | `slot` *(req)*, `chip` (default `auto`), `baud` (default `921600`), `flash_mode` (`dio`), `flash_freq` (`40m`), `flash_size` (`keep`), `erase` (`1`/`true`). Binaries: a `flash_args` part + the referenced `.bin` parts, **or** parts named `bin@<offset>` (e.g. `bin@0x10000`) | `ok`, `output` (esptool stdout+stderr), `returncode` |
| GET | `/api/firmware/list` | List stored firmware under `/var/lib/rfc2217/firmware/` | — | `ok`, `files[]` (`{project, filename, size, modified}`) |
| POST | `/api/firmware/upload` | *multipart/form-data.* Upload a `.bin` into a project dir | `project` (field), `file` (file part) | `ok`, `project`, `filename`, `size` |
| DELETE | `/api/firmware/delete` | Delete a stored firmware file | `project`, `filename` | `ok` |
| GET | `/firmware/<project>/<filename>` | Download a raw `.bin` (OTA serving; **not** under `/api/`) | path params | binary body |

See [Flash firmware](../how-to-guides/flash-firmware.md) for worked `curl` invocations of `/api/flash`.

---

## GPIO

| Method | Path | Purpose | Key request | Key response |
|---|---|---|---|---|
| POST | `/api/gpio/set` | Drive a Pi BCM GPIO pin | `pin` (int, must be allowed), `value` (`0`, `1`, or `"z"`) | `ok`, `pin`, `value` |
| GET | `/api/gpio/status` | State of the pins currently driven | — | `ok`, `pins` (`{ "<pin>": {direction, value} }`) |

**Allowed pins:** `16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27`. Anything else → `400`.
A value other than `0`/`1`/`"z"` → `400`. `"z"` = input with pull-up. Pins released to `"z"`
drop out of `/api/gpio/status`. See [Control device GPIO](../how-to-guides/control-device-gpio.md).

---

## Signal generator

All six return `503 {"ok": false, "error": "signal generator not available"}` if
hardware init failed at boot.

| Method | Path | Purpose | Key request | Key response |
|---|---|---|---|---|
| POST | `/api/siggen/start` | Start an RF carrier (optionally Morse-keyed) | `freq_hz` (or `freq`) *(req)*, `backend` (`auto`/`si5351`/`gpclk`), `channel` (0–2), `pin` (5/6), `atten_db`, `morse` (`{message, wpm, repeat}`) | `ok`, `active`, `backend`, `freq_hz`, `channel`, `pin`, `atten_db`, `morse` |
| POST | `/api/siggen/stop` | Stop the carrier (idempotent) | — | `ok` + state |
| GET | `/api/siggen/status` | State **and hardware detection** | — | `ok`, `active`, `backend`, `freq_hz`, …, `hardware` (`{si5351, gpclk, pe4302}` booleans) |
| POST | `/api/siggen/freq` | Retune without restarting the keyer | `freq_hz` (or `freq`) *(req)*, `channel` | `ok` + state |
| POST | `/api/siggen/atten` | Set PE4302 attenuation | `db` (0–31.5, 0.5 steps) *(req)* | `ok` + state (error if no PE4302) |
| GET | `/api/siggen/frequencies` | Achievable frequencies in a range | `low` (default 3500000), `high` (default 4000000), `backend` | `ok`, `frequencies[]` |

Because GPCLK only hits integer-divider frequencies, **trust the `freq_hz` in the
response, not the value you requested.** See [Use the signal generator](../how-to-guides/use-the-signal-generator.md).

---

## WiFi

`scan`, `ap_start`, `sta_join`, and `http` are disabled (return an error) while
the workbench is in **serial-interface** mode. See
[Run WiFi tests](../how-to-guides/run-wifi-tests.md).

| Method | Path | Purpose | Key request | Key response |
|---|---|---|---|---|
| GET | `/api/wifi/ping` | Firmware version + uptime | — | `ok`, `fw_version`, `uptime` |
| GET | `/api/wifi/mode` | Current mode | — | `ok`, `mode`, `ssid`, `ip` |
| POST | `/api/wifi/mode` | Switch mode | `mode` *(req)* (`wifi-testing`/`serial-interface`), `ssid`, `pass` | `ok`, `mode` |
| POST | `/api/wifi/ap_start` | Start a SoftAP on wlan0 | `ssid` *(req)*, `pass`, `channel` (default 6) | `ok`, `ip` (`192.168.4.1`) |
| POST | `/api/wifi/ap_stop` | Stop the SoftAP | — | `ok` |
| GET | `/api/wifi/ap_status` | AP state + connected stations | — | `ok`, `active`, `ssid`, `channel`, `stations[]` (`{mac, ip}`) |
| POST | `/api/wifi/sta_join` | Join a network as a station | `ssid` *(req)*, `pass`, `timeout` (default 15) | `ok`, `ip`, `gateway` |
| POST | `/api/wifi/sta_leave` | Disconnect the station | — | `ok` |
| POST | `/api/wifi/http` | Relay an HTTP request from the Pi | `method` (default GET), `url` *(req)*, `headers`, `body` (**base64**), `timeout` (default 10) | `ok`, `status`, `headers`, `body` (**base64**) |
| GET | `/api/wifi/scan` | Scan for networks (`iw`) | — | `ok`, `networks[]` (`{ssid, rssi, auth}`) |
| GET | `/api/wifi/events` | Drain or long-poll station events | `timeout` (s; 0 = drain only) | `ok`, `events[]` (`STA_CONNECT`/`STA_DISCONNECT`) |
| POST | `/api/wifi/lease_event` | **Internal** — dnsmasq lease callback | `action`, `mac`, `ip`, `hostname` | `ok` |

The AP is always `192.168.4.1/24`, DHCP `192.168.4.2`–`192.168.4.20`. The
`/api/wifi/http` relay is the **only** way to reach a device on the AP subnet
from your laptop — and its `body` is base64 in both directions.

---

## BLE

If `bleak` is not installed, scan/connect/disconnect/write return `501`; status
returns `{"ok": true, "state": "unavailable"}`.

| Method | Path | Purpose | Key request | Key response |
|---|---|---|---|---|
| POST | `/api/ble/scan` | Scan for peripherals | `timeout` (s, 0→5), `name_filter` | `ok`, `devices[]` (`{address, name, rssi}`) |
| POST | `/api/ble/connect` | Connect by address; enumerate GATT | `address` *(req)* | `ok`, `address`, `name`, `services[]` (`409` on failure) |
| POST | `/api/ble/disconnect` | Disconnect the current peripheral | — | `ok` |
| GET | `/api/ble/status` | Connection state | — | `ok`, `state` (`idle`/`scanning`/`connected`/`unavailable`), `address`, `name` |
| POST | `/api/ble/write` | Write hex bytes to a characteristic | `characteristic` *(req)*, `data` (hex) *(req)*, `response` (default `true`) | `ok`, `bytes_written` |

One connection at a time. See [Test BLE devices](../how-to-guides/test-ble-devices.md).

---

## Debug / GDB (OpenOCD)

| Method | Path | Purpose | Key request | Key response |
|---|---|---|---|---|
| POST | `/api/debug/start` | Start an OpenOCD session (auto-detects slot + chip via JTAG TAP-ID) | `slot`, `chip`, `probe` (all optional) | `ok`, `slot`, `chip`, `gdb_port`, `telnet_port`, `probe`, `gdb_target` |
| POST | `/api/debug/stop` | Stop the session | `slot` (optional; auto-finds active) | `ok`, `slot` |
| GET | `/api/debug/status` | Debug state for every slot | — | `ok`, `slots` (`{label: {debugging, chip, gdb_port, telnet_port, pid, probe}}`) |
| GET | `/api/debug/probes` | Configured ESP-Prog probes | — | `ok`, `probes[]` (`{label, type, in_use, slot}`) |
| GET | `/api/debug/group` | Slot groups for dual-USB configs | — | `ok`, `groups` |

`gdb_target` is returned as `target extended-remote <host_ip>:<gdb_port>` — the
workbench's detected IP (the same `host_ip` as the RFC2217 URLs), not a hardcoded
hostname.
OpenOCD auto-starts on hotplug for native-USB-JTAG chips (C3/C6/H2/S3). See
[Debug with GDB](../how-to-guides/debug-with-gdb.md).

---

## Logs, system, and the operator panel

| Method | Path | Purpose | Key request | Key response |
|---|---|---|---|---|
| GET | `/api/log` | Activity log (200-entry ring) | `since` (ISO ts) | `ok`, `entries[]` (`{ts, msg, cat}`) |
| GET | `/api/udplog` | Buffered UDP log lines (port 5555) | `since` (epoch), `source` (IP), `limit` (default 200) | `ok`, `lines[]` (`{ts, source, line}`) |
| DELETE | `/api/udplog` | Clear the UDP log buffer | — | `ok` |
| POST | `/api/human-interaction` | **Blocks** until the operator clicks Done/Cancel on the panel (or times out) | `message` *(req)*, `timeout` (s, default 120) | `ok`, `confirmed`, `timeout` |
| GET | `/api/human/status` | Is a prompt pending? (UI poll) | — | `ok`, `pending`, `message` |
| POST | `/api/human/done` | Confirm the pending prompt | — | `ok` |
| POST | `/api/human/cancel` | Cancel the pending prompt | — | `ok` |
| GET | `/api/test/progress` | Current test session (UI poll) | — | `ok`, `active`, `spec`, `phase`, `total`, `completed[]`, `current` |
| POST | `/api/test/update` | Push test progress | start/step/result/end fields | `ok` |
| POST | `/api/enter-portal` | Background: join a device's captive-portal AP and submit WiFi creds | `ssid` *(req)*, `password`, `portal_ssid`, `portal_ip` | `ok`, `message` |
| GET | `/` , `/index.html` | The dashboard web UI | — | HTML |

Only one `/api/human-interaction` prompt can be pending at a time (`409`
otherwise). See [Run the test suite](../how-to-guides/run-the-test-suite.md).

---

## Not exposed by the API

These exist as code or skills but have **no HTTP endpoint** — don't write
automation against them:

- **MQTT** — the installer runs a Mosquitto broker on `1883/tcp` (anonymous), but
  the portal has **no `/api/mqtt/*` routes**. Devices and `mosquitto_pub`/`sub`
  talk to the broker directly. See [Configuration files](configuration-files.md#mosquitto-broker).
- **Packet sniffer** — `sniffer.py` exists but is not wired to any route.
- `serial_proxy.py` is a standalone CLI; the portal runs `plain_rfc2217_server.py` per slot.
