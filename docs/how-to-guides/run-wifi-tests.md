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

# Run WiFi tests

Drive the Pi's `wlan0` as a WiFi test instrument: stand up an access point, join
a network as a station, scan, relay HTTP to a device, and watch stations come
and go. Use this to exercise a DUT's WiFi stack end to end. All control is over
`http://pi4b.local:8080` — never SSH.

## Mode: instrument vs LAN uplink

`wlan0` has two modes. Check the current one:

```bash
curl -s http://pi4b.local:8080/api/wifi/mode | jq .
# → {"ok":true,"mode":"wifi-testing","ssid":null,"ip":null}
```

| Mode | What `wlan0` does | Instrument endpoints |
|---|---|---|
| `wifi-testing` (default) | Acts as the test instrument (AP/station/scan/relay) | Available |
| `serial-interface` | Joins **your** LAN to give a Pi without Ethernet connectivity | **Disabled** |

Switch to instrument mode:

```bash
curl -s -X POST http://pi4b.local:8080/api/wifi/mode \
  -H 'Content-Type: application/json' -d '{"mode":"wifi-testing"}'
```

Switch to LAN uplink (the workbench joins your network):

```bash
curl -s -X POST http://pi4b.local:8080/api/wifi/mode \
  -d '{"mode":"serial-interface","ssid":"MyLAN","pass":"secret"}'
```

A **failed** mode switch auto-reverts to `wifi-testing`, so you never get stranded.
See [operating modes](../explanation/architecture-overview.md#operating-modes).

## SoftAP: be the access point

Start an AP. It is always `192.168.4.1/24`, with DHCP handing out
`192.168.4.2`–`192.168.4.20`:

```bash
curl -s -X POST http://pi4b.local:8080/api/wifi/ap_start \
  -H 'Content-Type: application/json' \
  -d '{"ssid":"TestAP","pass":"testpass123","channel":6}'
# → {"ok":true,"ip":"192.168.4.1"}
```

See who joined:

```bash
curl -s http://pi4b.local:8080/api/wifi/ap_status | jq .
# → {"ok":true,"active":true,"ssid":"TestAP","channel":6,
#    "stations":[{"mac":"a4:cf:...","ip":"192.168.4.2"}]}
```

Stop it:

```bash
curl -s -X POST http://pi4b.local:8080/api/wifi/ap_stop
```

## Station mode: join a network

Join the DUT's AP (or any network) as a client:

```bash
curl -s -X POST http://pi4b.local:8080/api/wifi/sta_join \
  -d '{"ssid":"DeviceAP","pass":"devicepass","timeout":15}'
# → {"ok":true,"ip":"192.168.4.7","gateway":"192.168.4.1"}

curl -s -X POST http://pi4b.local:8080/api/wifi/sta_leave
```

**AP and station modes are mutually exclusive** — running one stops the other.

## Scan

```bash
curl -s http://pi4b.local:8080/api/wifi/scan | jq '.networks'
# → [{"ssid":"...","rssi":-48,"auth":"WPA2"}, ...]
```

Results are sorted strongest-first by RSSI. Your own SoftAP is **not** listed.

## HTTP relay: the only way to reach a device on `192.168.4.x`

A device on the AP subnet is **not** directly reachable from your laptop. Relay
the request through the Pi:

```bash
curl -s -X POST http://pi4b.local:8080/api/wifi/http \
  -H 'Content-Type: application/json' \
  -d '{"method":"GET","url":"http://192.168.4.2/status","timeout":10}'
```

**Critical: the `body` field is base64 in BOTH directions** — base64-encode any
request body you send, and decode the response body:

```bash
curl -s -X POST http://pi4b.local:8080/api/wifi/http \
  -d '{"method":"GET","url":"http://192.168.4.2/status"}' \
  | jq -r .body | base64 -d
```

To POST a JSON body to the device, base64-encode it first:

```bash
B=$(printf '{"led":true}' | base64)
curl -s -X POST http://pi4b.local:8080/api/wifi/http \
  -d "{\"method\":\"POST\",\"url\":\"http://192.168.4.2/set\",\"body\":\"$B\"}"
```

## Station events: confirm a device joined or left

Long-poll for connect/disconnect events. `timeout=0` drains the queue without
waiting:

```bash
curl -s "http://pi4b.local:8080/api/wifi/events?timeout=30" | jq '.events'
# → [{"event":"STA_CONNECT","mac":"a4:cf:...","ip":"192.168.4.2"}]
```

This is the clean way to assert "the DUT reconnected within N seconds."

## Captive-portal provisioning

Hand a device its WiFi credentials by joining its setup AP and submitting the
form for it. This runs **in the background**:

```bash
curl -s -X POST http://pi4b.local:8080/api/enter-portal \
  -H 'Content-Type: application/json' \
  -d '{"ssid":"<workbench AP ssid>","password":"<workbench AP pass>",
       "portal_ssid":"<the DUT setup AP>","portal_ip":"192.168.4.1"}'
```

Watch progress in the activity log:

```bash
curl -s http://pi4b.local:8080/api/log | jq '.entries[-10:]'
```

## Recipe: WiFi on/off reconnect test

Verify a device reconnects after losing its AP:

```bash
W=http://pi4b.local:8080
curl -s -X POST $W/api/wifi/ap_start -d '{"ssid":"TestAP","pass":"testpass123"}'
# (wait for the DUT to join — poll /api/wifi/ap_status or /api/wifi/events)

curl -s -X POST $W/api/wifi/ap_stop          # device loses WiFi
curl -s -X POST $W/api/wifi/ap_start -d '{"ssid":"TestAP","pass":"testpass123"}'

curl -s "$W/api/wifi/events?timeout=30" | jq '.events'   # expect STA_CONNECT
```

## Gotchas

- The WiFi-instrument endpoints (`scan`, `ap_start`, `sta_join`, `http`) **error
  out** while in `serial-interface` mode. Switch back to `wifi-testing` first.
- Devices on `192.168.4.x` are **not** directly reachable from your laptop — use
  the `/api/wifi/http` relay.
- The relay `body` is base64 **both ways**. Forgetting to decode the response is
  the most common surprise.
- AP and station are mutually exclusive; starting one tears down the other.

## Related

- [Monitor output and read logs](monitor-output-and-logs.md)
- [Architecture overview](../explanation/architecture-overview.md)
- [REST API](../reference/rest-api.md)
