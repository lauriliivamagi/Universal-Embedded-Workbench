---
type: how-to-guide
domain: complicated
audience: operator
stability: tactical
authority:
  provenance: institutional
  verifiability: testable
  evidence: strong
  currency: dated
epistemic-layer: method
---

# Run the test suite

Run the repo's pytest end-to-end suite, which drives the workbench over the HTTP
API. Reach for this to validate a workbench after setup or after a code update,
or to verify a board behaves end-to-end (flash, serial, WiFi, debug).

## Install and run

```bash
pip install -r requirements-dev.txt
```

Run **instrument-only** tests (no board required):

```bash
pytest pytest/ --wt-url http://pi4b.local:8080
```

Run the **full** suite (a board must be connected):

```bash
pytest pytest/ --wt-url http://pi4b.local:8080 --run-dut
```

## Options

| Option / env var | Effect |
|---|---|
| `--wt-url` | Workbench base URL. Default = env `WORKBENCH_URL`, else `http://localhost:8080`. |
| `--run-dut` | Flag. Tests marked `requires_dut` are **skipped** unless this is set. |
| `WIFI_TEST_STA_SSID` | SSID for station-mode (STA) tests. |
| `WIFI_TEST_STA_PASS` | Password for station-mode tests. |
| `WIFI_TEST_HTTP_URL` | Target URL the STA-mode HTTP-relay tests fetch. |

STA-mode tests need all three `WIFI_TEST_*` env vars **in addition to**
`--run-dut`.

## Run a subset

```bash
pytest pytest/workbench_test.py -k TestEndToEnd --run-dut --wt-url http://pi4b.local:8080
```

## Test ID map

| ID range | Area | Needs |
|---|---|---|
| WT-1xx | Protocol / ping | — |
| WT-2xx | SoftAP | — |
| WT-3xx | Station events | DUT |
| WT-4xx | STA mode | DUT + `WIFI_TEST_*` env |
| WT-5xx | HTTP relay | DUT |
| WT-6xx | Scan | — |
| WT-7xx | MQTT broker | — |
| WT-8xx | WiFi sniffer (capture AP) | — |
| WT-13xx | Signal generator | — |
| WT-14xx | USB-JTAG debug | — |
| WT-17xx | Auto-debug | — |
| WT-18xx | End-to-end flash + debug | DUT, **ordered** |
| WT-19xx | Serial buffer + multi-slot detection | — |

WT-18xx are **order-dependent**: `WT-1800` (the flash) must pass before the rest
run, and they use the pre-built binaries in `firmware/debug-test/output/<chip>/`.

## How the tests interact with you

The suite calls the API through the **`WorkbenchDriver`** wrapper
(`pytest/workbench_driver.py`) — not raw `curl` — and reports progress to the
Pi's dashboard panel via `/api/test/update`.

Some tests **pause for a human**. `POST /api/human-interaction` **blocks** until
the operator clicks **Done** (or **Cancel**) on the workbench web UI modal, or it
times out (default 120 s). Only **one** prompt can be pending at a time (`409`
otherwise). Behind the scenes the UI polls `/api/human/status`, confirms with
`/api/human/done`, and cancels with `/api/human/cancel`.

**Keep the dashboard open at `http://pi4b.local:8080` when running
`--run-dut`** so you can answer these prompts — a blocked prompt is the test
waiting on *you*.

## Gotchas

- **Without `--run-dut`, DUT tests are skipped, not failed.** A green run with no
  flag means only the instrument-only tests ran.
- **WT-18xx are order-dependent** — don't `-k` your way into running a later
  WT-18xx test without `WT-1800` having passed first.
- **A pending human-interaction prompt means a test is waiting on you** at
  `http://pi4b.local:8080` — click Done/Cancel or it will time out.

## Related

- [Tutorial 2 — Your first flash and test](../tutorials/02-your-first-flash-and-test.md)
- [REST API](../reference/rest-api.md)
- [Monitor output and read logs](monitor-output-and-logs.md)
