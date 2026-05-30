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

# Test BLE devices

Scan for, connect to, and write to a BLE peripheral through the workbench, which
bridges its onboard Bluetooth radio (`hci0`) to HTTP. Reach for this when you
need to drive a device's BLE interface from your laptop without a separate
Bluetooth dongle.

## Prerequisites: power on the radio

BLE rides the Pi's onboard Bluetooth (`hci0`), which must be powered on. If scans
come back empty, bring the radio up on the Pi (this is **host maintenance**, not
workbench operation):

```bash
sudo rfkill unblock bluetooth && sudo hciconfig hci0 up && sudo bluetoothctl power on
```

If the `bleak` library isn't installed, every BLE endpoint returns **HTTP 501**
and `GET /api/ble/status` reports state `"unavailable"`.

## Scan for peripherals

```bash
curl -s -X POST http://pi4b.local:8080/api/ble/scan \
  -H 'Content-Type: application/json' \
  -d '{"timeout":5,"name_filter":"MyDev"}' | jq
```

Returns `{ "devices": [ {"address","name","rssi"}, … ] }`. The optional
`name_filter` keeps only devices whose name contains the substring.

## Connect (enumerates GATT)

```bash
curl -s -X POST http://pi4b.local:8080/api/ble/connect \
  -H 'Content-Type: application/json' \
  -d '{"address":"AA:BB:CC:DD:EE:FF"}' | jq
```

Returns `{ "address", "name", "services": [ {"uuid","characteristics":[…]}, … ] }`.
Returns **HTTP 409** if the connection fails or one is already active — only
**one** connection at a time.

## Write a characteristic (hex bytes)

```bash
curl -s -X POST http://pi4b.local:8080/api/ble/write \
  -H 'Content-Type: application/json' \
  -d '{"characteristic":"6e400002-b5a3-f393-e0a9-e50e24dcca9e","data":"48656c6c6f","response":true}' | jq
```

Returns `{ "bytes_written": N }`. The `data` field is a **hex string** —
`"48656c6c6f"` is `"Hello"`. Set `"response": false` for a write-without-response.

## Check status / disconnect

```bash
curl -s http://pi4b.local:8080/api/ble/status | jq
# -> { "state": "idle|scanning|connected|unavailable", "address": …, "name": … }

curl -s -X POST http://pi4b.local:8080/api/ble/disconnect | jq
```

## Nordic UART Service (NUS) UUIDs

A common pattern for ESP32 BLE firmware is the Nordic UART Service:

| Role | UUID |
|---|---|
| Service | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| RX (write to the device) | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` |
| TX (notifications from the device) | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` |

## Recipe: send a command over NUS

1. **Scan** with a `name_filter` to find your device's address.
2. **Connect** by address; confirm the NUS service appears in `services[]`.
3. **Write** your command (hex-encoded) to the NUS RX characteristic
   `6e400002-b5a3-f393-e0a9-e50e24dcca9e`.
4. **Disconnect** to free the radio for the next device.

## Gotchas

- **One connection at a time** — connecting while another session is active
  returns `409`. Disconnect first.
- **Hex-encode the `data` field** — it's a hex string, not raw text.
- **The radio is `hci0` and must be powered on** — empty scans usually mean the
  Bluetooth radio is blocked or down; see the prerequisite above.
- **No `bleak` → `501`** on all endpoints and `"state": "unavailable"` on status.

## Related

- [REST API](../reference/rest-api.md)
- [Monitor output and read logs](monitor-output-and-logs.md)
