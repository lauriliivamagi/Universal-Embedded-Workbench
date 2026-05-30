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

# Use the signal generator

Emit an RF carrier or a Morse (CW) beacon from the workbench, and optionally
attenuate it. Use this to feed a known signal into a DUT's receiver or antenna
path. All control is over `http://pi4b.local:8080` — never SSH.

## Always check the hardware first

The available backend and frequency precision depend on what's wired. Start
here:

```bash
curl -s http://pi4b.local:8080/api/siggen/status | jq .
# → {"ok":true,"active":false,"backend":"si5351","freq_hz":0,"channel":0,
#    "pin":null,"atten_db":0,"morse":null,
#    "hardware":{"si5351":true,"gpclk":true,"pe4302":false}}
```

Read the `hardware` block before you do anything:

| Detection | What it means |
|---|---|
| `si5351: true` | Exact frequencies, ~8 kHz – 160 MHz. Preferred. |
| `si5351: false` | GPCLK fallback only — **integer-divider frequencies** (coarse steps). |
| `pe4302: false` | No attenuator. `/api/siggen/atten` will error. |

With GPCLK you rarely get exactly what you ask for, so **trust the `freq_hz` in
the response, not the value you requested.**

## Start a carrier

```bash
curl -s -X POST http://pi4b.local:8080/api/siggen/start \
  -H 'Content-Type: application/json' \
  -d '{"freq_hz":3500000,"backend":"auto","channel":0,"atten_db":0}'
# → {"ok":true,"active":true,"backend":"si5351","freq_hz":3500000,
#    "channel":0,"pin":null,"atten_db":0,"morse":null}
```

| Field | Values | Notes |
|---|---|---|
| `freq_hz` | integer Hz | Required (alias: `freq`). |
| `backend` | `auto` / `si5351` / `gpclk` | `auto` picks Si5351 if present. |
| `channel` | `0`–`2` | Si5351 CLK output. |
| `pin` | `5` or `6` | GPCLK output pin. |
| `atten_db` | `0`–`31.5` | Applied via PE4302 if present. |

The response **echoes the actual state** — trust its `freq_hz`, especially on
GPCLK.

## Morse / CW beacon

Add a `morse` block to `/start` to key the carrier instead of leaving it
continuous (PARIS timing, `wpm` 1–60):

```bash
curl -s -X POST http://pi4b.local:8080/api/siggen/start \
  -d '{"freq_hz":7030000,"backend":"auto",
       "morse":{"message":"VVV DE TEST","wpm":15,"repeat":true}}'
```

Omit `morse` for a plain continuous carrier. A new `/start` **replaces** the
previous signal.

## Retune without restarting the keyer

To change frequency while a Morse beacon keeps running:

```bash
curl -s -X POST http://pi4b.local:8080/api/siggen/freq \
  -d '{"freq_hz":7100000,"channel":0}'
```

## Attenuation (needs a PE4302)

```bash
curl -s -X POST http://pi4b.local:8080/api/siggen/atten \
  -d '{"db":12.5}'
```

Range `0`–`31.5` dB in `0.5` dB steps. Errors if no PE4302 was detected
(`hardware.pe4302: false`).

## List achievable frequencies in a band

Especially useful on GPCLK, where only certain frequencies are reachable:

```bash
curl -s "http://pi4b.local:8080/api/siggen/frequencies?low=3500000&high=4000000&backend=auto" \
  | jq '.frequencies'
```

Pick a value from `frequencies[]` and feed it back into `/start` or `/freq`.

## Stop

```bash
curl -s -X POST http://pi4b.local:8080/api/siggen/stop
```

Idempotent — safe to call when nothing is running.

## Gotchas

- **Single instance only.** A new `/start` replaces the old signal; there is no
  multi-channel mixing.
- **PE4302 LE shares GPIO6 with GPCLK2.** If GPCLK runs on pin 6, the
  attenuator's latch is unavailable. To use a carrier **and** live attenuation
  together, use **Si5351**, or run GPCLK on **pin 5** — see
  [pin conflicts](../reference/hardware-and-wiring.md#pin-conflicts-you-must-respect).
- Wiring config lives at `/etc/rfc2217/signalgen.json` — see
  [Configuration files](../reference/configuration-files.md).
- **All six siggen endpoints return `503`** if the hardware didn't initialize at
  boot. Check `/api/siggen/status` first.
- Don't `sleep` blindly waiting for a signal to take effect — **poll the DUT**
  for the expected response instead.

## Related

- [Hardware and wiring](../reference/hardware-and-wiring.md)
- [Configuration files](../reference/configuration-files.md)
- [REST API](../reference/rest-api.md)
