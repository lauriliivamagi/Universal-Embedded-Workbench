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

# Monitor output and read logs

See what a board is printing — over serial, over the network, or from the
workbench's own activity log. Reach for serial when you're watching a board boot
or crash; reach for UDP logs when the board is on WiFi and you want non-blocking,
multi-device capture.

## Which one do I use?

| Use… | When you're… |
|---|---|
| **Serial** | Watching boot/crash output, working with a board not on WiFi, or waiting for a specific line. |
| **UDP logs** | Working with a board on WiFi, capturing many devices at once, want non-blocking capture, or watching during an OTA update. |
| **Activity log** | Watching what the *workbench* is doing — recovery, flashing, WiFi events. |

You can mix all three at once.

## Serial

Three non-exclusive tools. Serial baud is fixed at **115200** for the
server-side reads.

### Wait for a pattern

`POST /api/serial/monitor` reads a server-side buffer and optionally blocks until
a substring appears. It does **not** tie up the port, so it's safe to run
alongside a live session.

```bash
curl -s -X POST http://pi4b.local:8080/api/serial/monitor \
  -H 'Content-Type: application/json' \
  -d '{"slot":"SLOT1","pattern":"app_main","timeout":10}' \
  | jq '{matched, line}'
```

Returns `{ok, matched, line, output[]}`. `timeout` defaults to `10` seconds.

### Passive tail

`GET /api/serial/output` reads the slot's ring buffer without disturbing the
proxy — poll it to keep watching:

```bash
curl -s "http://pi4b.local:8080/api/serial/output?slot=SLOT1&lines=50" \
  | jq -r '.lines[] | "\(.ts) \(.text)"'
```

Pass `&since=<epoch>` to fetch only lines newer than a timestamp. Returns
`{lines:[{ts,text}]}`.

### Live interactive session (RFC2217)

For a real terminal, open the slot's serial port over RFC2217. **Only one client
at a time** per slot. Always include `?ign_set_control`:

```bash
# PlatformIO
pio device monitor --port 'rfc2217://pi4b.local:4001?ign_set_control'

# ESP-IDF
idf.py -p 'rfc2217://pi4b.local:4001?ign_set_control' monitor

# pyserial
python3 -c "import serial; serial.serial_for_url('rfc2217://pi4b.local:4001?ign_set_control', baudrate=115200)"

# socat — expose it as a local PTY
socat pty,link=/dev/ttyESP32,raw,echo=0 tcp:pi4b.local:4001 &
```

Why `?ign_set_control` is required:
[Serial reset and download mode](../explanation/serial-reset-and-download-mode.md).

## UDP logs

Firmware sends log lines to **UDP 5555**; the workbench keeps a 2000-line ring
buffer. The firmware must be configured to log to `pi4b.local:5555`.

```bash
# Read recent lines, optionally filtered by source IP
curl -s "http://pi4b.local:8080/api/udplog?limit=200&source=192.168.4.5" \
  | jq -r '.lines[] | "\(.ts) \(.source) \(.line)"'

# Only lines newer than a timestamp
curl -s "http://pi4b.local:8080/api/udplog?since=1717000000" \
  | jq -r '.lines[].line'

# Clear the buffer
curl -s -X DELETE http://pi4b.local:8080/api/udplog | jq .ok
```

Returns `{lines:[{ts,source,line}]}`. `limit` defaults to `200`.

## The portal's activity log

`GET /api/log` is the workbench's own 200-entry ring buffer — good for watching
recovery, flashing, and WiFi events as they happen:

```bash
curl -s "http://pi4b.local:8080/api/log" \
  | jq -r '.entries[] | "\(.ts) [\(.cat)] \(.msg)"'
```

Pass `?since=<iso-ts>` for entries after a timestamp. Returns
`{entries:[{ts,msg,cat}]}`; `cat` is one of `info`, `ok`, `error`, `step`.

```bash
# Watch only errors
curl -s "http://pi4b.local:8080/api/log" \
  | jq -r '.entries[] | select(.cat=="error") | "\(.ts) \(.msg)"'
```

## Gotchas / Troubleshooting

- **One live RFC2217 client per slot.** If a tool reports "port busy", a stale
  session is still holding it — close it and retry.
- **Dual-USB boards:** monitor the **UART** slot, not the JTAG slot, or you'll
  see nothing useful.
- **Nothing prints?** Confirm the baud is **115200** and kick the board with
  `POST /api/serial/reset` to capture the boot banner.
- **UDP log empty?** The firmware isn't pointed at `pi4b.local:5555`, or the
  board isn't on the network yet.

## Related

- [Flash firmware](flash-firmware.md)
- [Recover a stuck or flapping device](recover-a-stuck-or-flapping-device.md)
- [REST API](../reference/rest-api.md)
- [Slot states and network ports](../reference/slot-states-and-network-ports.md)
