---
name: saleae-logic-mcp
description: Drive a Saleae logic analyzer through the Logic 2 MCP server (HTTP 127.0.0.1:10530) — list devices, start/stop timed/manual/trigger captures, add built-in protocol analyzers (SPI/I2C/UART), and export CSV/binary/data-table results, all via MCP tool calls (no Python, no curl). Use whenever the user wants to capture or decode digital signals with a Saleae device from the agent. Triggers on "Saleae", "Logic 2", "logic analyzer", "Logic16", "logic capture", "decode SPI/I2C/UART", "protocol analyzer", "MCP capture", "logic2 mcp", "export raw data", "data table", "add_analyzer", "get_devices", ".sal capture".
---

# Saleae Logic Analyzer over MCP

Control the Logic 2 application through its **MCP server** using the `logic2` MCP tools. This is the
preferred way to drive a Saleae logic analyzer from the agent. For the gRPC/scripting route (writing a
standalone Python program), use the `saleae-logic-python` skill instead.

**Golden rule:** Use the `logic2` MCP tools directly. Do **not** poke the server with `curl` and do
**not** write Python — those are the other skill's job. Every capture you open must eventually be
closed with `close_capture`.

## Bench reality (this workbench)

- The Saleae plugs into the **laptop**, and Logic 2 runs on the **laptop** — *not* the Pi. This is the
  one bench tool that is not Pi-hosted. Probes reach the DUT at the bench; samples land on the laptop.
- Verified device: **Logic16**, serial `921D7A68A3E27E70`, `isSimulation:false`.
- Logic 2 (v2.4.44) MCP fully drives the original Logic16, even though Saleae's docs only list
  Logic 8 / Pro 8 / Pro 16. MCP support is marked **experimental** by Saleae.

## 1. Verify the server is up

The MCP server listens on `http://127.0.0.1:10530` by default. Enable it in Logic 2 →
**Settings > Automation > Enable MCP Server** (or the **Automation** button in the bottom bar).

If the `logic2` MCP tools are missing from your tool list, the server was down when the session
started — enable it in Logic 2, then **Reconnect** the `logic2` server from the Claude Code MCP panel.
A "failed / Unable to connect" status means nothing is listening on 10530 (server not enabled), *not*
an auth problem — the server needs no auth.

Confirm hardware with `get_devices` before capturing:

```
get_devices { "includeSimulationDevices": false }
→ {"devices":[{"deviceId":"921D7A68A3E27E70","deviceType":"Logic16","isSimulation":false}]}
```

## 2. Capture lifecycle

`start_capture` → `wait_capture` (timed/trigger) **or** `stop_capture` (manual) → export/save →
`close_capture`. `start_capture` returns a `captureId`; thread it through every later call.

**Timed capture (most common):**

```
start_capture {
  "deviceId": "921D7A68A3E27E70",
  "logicDeviceConfiguration": { "logicChannels": { "digitalChannels": [0,1] }, "digitalSampleRate": 16000000 },
  "captureConfiguration": { "bufferSizeMegabytes": 128, "timedCaptureMode": { "durationSeconds": 0.2 } }
}
→ {"captureId": 3}

wait_capture  { "captureId": 3 }
save_capture  { "captureId": 3, "filepath": "/tmp/cap.sal" }
close_capture { "captureId": 3 }
```

**Manual capture:** use `captureConfiguration.manualCaptureMode` (or omit `captureConfiguration`),
then `stop_capture` instead of `wait_capture`. Never use `wait_capture` and `stop_capture` on the
same capture.

**Trigger capture:** `captureConfiguration.digitalCaptureMode` with `triggerType` (0=rising, 1=falling,
2=pulse-high, 3=pulse-low), `triggerChannelIndex`, and optional `afterTriggerSeconds`. `wait_capture`
blocks until the trigger fires and the post-trigger window completes.

## 3. Decode a protocol

> ⚠️ **MCP `settings` values are tagged-union objects, not bare scalars (verified 2026-06).** Each
> value must be wrapped: `{"numberValue": N}`, `{"stringValue": "..."}`, or `{"boolValue": true}`.
> Passing a bare `0` / `"..."` (the form the **gRPC/Python** API uses) makes the server reply with the
> misleading `Invalid value type for analyzer setting "<name>", expected number`. This MCP↔Python
> encoding difference is the #1 gotcha — get it right and `add_analyzer` works fine here on the Logic16.

Add a built-in analyzer **after** the capture has data:

```
add_analyzer { "captureId": 3, "analyzerName": "SPI", "analyzerLabel": "spi0",
  "settings": {
    "MISO":  {"numberValue": 0},
    "Clock": {"numberValue": 1},
    "Enable":{"numberValue": 2},
    "Bits per Transfer": {"stringValue": "8 Bits per Transfer (Standard)"}
  } }
→ {"analyzerId": <n>}

export_data_table_csv { "captureId": 3, "filepath": "/tmp/spi.csv", "analyzers": [<analyzerId>] }
```

`analyzerName` and every setting key + the value inside the wrapper must match the Logic 2 **Add
Analyzer** dialog **exactly** (capitalization, spacing, the full option string). Verified
**Async Serial (UART)** settings — note each value is wrapped:

```
"settings": {
  "Input Channel": {"numberValue": 1}, "Bit Rate (Bits/s)": {"numberValue": 115200},
  "Bits per Frame": {"stringValue": "8 Bits per Transfer (Standard)"},
  "Stop Bits": {"stringValue": "1 Stop Bit (Standard)"},
  "Parity Bit": {"stringValue": "No Parity Bit (Standard)"},
  "Significant Bit": {"stringValue": "Least Significant Bit Sent First (Standard)"},
  "Signal inversion": {"stringValue": "Non Inverted (Standard)"}, "Mode": {"stringValue": "Normal"}
}
```

## 4. Export raw samples

```
export_raw_data_csv { "captureId": 3, "directory": "/tmp/raw", "logicChannels": { "digitalChannels": [0,1] }, "analogDownsampleRatio": 1 }
```

The `directory` must already exist and is a folder (no filename); it produces `digital.csv` /
`analog.csv`. Use `export_raw_data_binary` for the binary format.

## Gotchas (verified live, not in the docs)

- **Omit `digitalThresholdVolts` on the Logic16.** Scalar endpoints like `3.3` or `1.8` are rejected
  ("options: 1.8V to 3.6V, 3.6V to 5.0V"); leaving it out uses the device default, which covers 3.3 V
  logic. Other devices accept a scalar fine.
- `save_capture` / `load_capture` use `filepath` (lowercase **p**).
- `export_raw_data_csv` / `_binary` **require** `analogDownsampleRatio` (use `1`) even for digital-only
  captures.
- Pass `captureId` to `wait_capture` / `stop_capture` / `save_capture` / `close_capture` — omitting it
  errors with "should have required property 'captureId'".
- Read each error message closely — API-misuse errors list the valid options inline.
- **`add_analyzer` settings must be wrapped** as `{"numberValue"|"stringValue"|"boolValue": …}`, unlike
  the bare values the Python API uses — bare values give a misleading `expected number` error (see §3).

See [references/reference.md](references/reference.md) for the full tool table with exact argument schemas.
