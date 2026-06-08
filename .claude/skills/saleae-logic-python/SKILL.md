---
name: saleae-logic-python
description: Write and run Python automation scripts for a Saleae logic analyzer using the logic2-automation package over gRPC (127.0.0.1:10430) — connect/launch Logic 2, configure channels, run timed/manual/digital-trigger captures, add built-in protocol analyzers (SPI/I2C/UART), export CSV/binary/data-table, and handle capture errors. Use when the user wants a standalone/repeatable capture script, CI/headless capture, or programmatic control beyond ad-hoc MCP calls. Triggers on "saleae python", "logic2-automation", "automation API", "capture script", "automate Saleae", "Manager.connect", "start_capture script", "gRPC capture", "headless Saleae", "xvfb logic", "Logic16 python".
---

# Saleae Logic Analyzer — Python Automation API

Write Python that drives Logic 2 through the **`logic2-automation`** package (gRPC on
`127.0.0.1:10430`). Use this when the user wants a reusable script, headless/CI capture, or logic
that's awkward as one-shot tool calls. For interactive, one-off agent-driven captures, prefer the
`saleae-logic-mcp` skill (same backend, no script to maintain).

**Golden rule:** Always use the `with` lifecycle so connections and captures close cleanly:
`with automation.Manager.connect(...) as manager:` and `with manager.start_capture(...) as capture:`.
A capture must be `wait()`-ed (timed/trigger) or `stop()`-ed (manual) before you save/export it.

## Bench reality (this workbench)

- The Saleae plugs into the **laptop**, and Logic 2 runs on the **laptop** — *not* the Pi. Run the
  script on the laptop (or anything that can reach the laptop's gRPC port).
- Verified device: **Logic16**, serial `921D7A68A3E27E70` (`DeviceType.LOGIC_16`). `LOGIC_16` is a
  first-class device type in the automation API.

## Setup

```bash
pip install logic2-automation   # needs Logic 2 ≥ 2.4.0; docs say Python 3.8–3.10
```

> Verified 2026-06: `logic2-automation 1.0.11` installs and runs fine on **Python 3.14** too
> (grpcio 1.81 ships cp314 wheels) — the 3.8–3.10 ceiling in the docs is stale. A throwaway venv
> (`python3 -m venv … && …/pip install logic2-automation`) is enough.
>
> **Settings format differs from MCP.** This Python/gRPC API takes **bare** setting values
> (`settings={"MISO": 0, "Bits per Transfer": "8 Bits…"}`). The `saleae-logic-mcp` route takes the
> same settings but each value **wrapped** as `{"numberValue": 0}` / `{"stringValue": "…"}`. Both
> decode fine; don't copy bare values into an MCP call (you'll get a misleading `expected number`).

Enable the gRPC server: Logic 2 → **Settings > Automation > Enable Automation Server** (port
`10430`), or launch with the flag: `./Logic-2.4.44.AppImage --automation [--automationPort N]`.

## Minimal capture → decode → export

```python
from saleae import automation

with automation.Manager.connect(port=10430) as manager:
    dev = automation.LogicDeviceConfiguration(
        enabled_digital_channels=[0, 1, 2],
        digital_sample_rate=16_000_000,
        # NOTE: omit digital_threshold_volts on the Logic16 (see Gotchas)
    )
    cap_cfg = automation.CaptureConfiguration(
        capture_mode=automation.TimedCaptureMode(duration_seconds=0.2)
    )
    # device_id omitted → first real (non-sim) device; or pass "921D7A68A3E27E70"
    with manager.start_capture(device_configuration=dev, capture_configuration=cap_cfg) as cap:
        cap.wait()                                   # timed/trigger; use cap.stop() for manual
        spi = cap.add_analyzer("SPI", label="spi0", settings={
            "MISO": 0, "Clock": 1, "Enable": 2,
            "Bits per Transfer": "8 Bits per Transfer (Standard)",
        })
        cap.export_data_table(filepath="/tmp/spi.csv", analyzers=[spi])
        cap.export_raw_data_csv(directory="/tmp/raw", digital_channels=[0, 1, 2])
        cap.save_capture(filepath="/tmp/cap.sal")
    # capture auto-closes here
```

## Async Serial (UART) analyzer — verified settings

These `add_analyzer("Async Serial", …)` settings were accepted first try (Logic 2 v2.4.44) and
decoded a 115200 8N1 ESP32 UART cleanly. Values are bare here (Python API); for MCP, wrap each as
`{"numberValue": …}` / `{"stringValue": …}`:

```python
uart = cap.add_analyzer("Async Serial", label="rx", settings={
    "Input Channel": 1,                                       # int
    "Bit Rate (Bits/s)": 115200,                             # int
    "Bits per Frame": "8 Bits per Transfer (Standard)",
    "Stop Bits": "1 Stop Bit (Standard)",
    "Parity Bit": "No Parity Bit (Standard)",
    "Significant Bit": "Least Significant Bit Sent First (Standard)",
    "Signal inversion": "Non Inverted (Standard)",
    "Mode": "Normal",
})
cap.export_data_table(filepath="/tmp/uart.csv", analyzers=[uart])
```

`export_data_table` emits one row per decoded frame (`name,type,start_time,duration,data`), one
character per row — reassemble by concatenating the `data` column in time order. An idle UART line
sits HIGH; an all-LOW capture means a missing/wrong ground, not a decode problem.

> **Decode binary from the analyzer, not the data-table text.** The `data` column renders control
> bytes lossily (e.g. `0x04`/`0x03` both show as `.`, `0x00` as `\0`), so it's fine for ASCII but
> unusable for binary protocol frames. To recover exact bytes: keep the data table only for the
> per-byte **`start_time`** (authoritative framing), then re-sample the byte value from the raw
> waveform at `start_time + bit*(1.5 + b)` for b in 0..7 (`bit = 1/baud`). This beats a hand-rolled
> raw decoder, which mis-frames by re-triggering on every high→low edge instead of skipping a full
> 10-bit frame.

## Glitch filter — clean up reset spikes

Capturing across a DUT reset (e.g. `POST /api/serial/reset` to record a boot command burst) picks
up sub-microsecond settling spikes on the lines that a raw decoder reads as phantom start bits. Add
a **glitch filter** in the device config — it's a first-class field in the automation API, not just
the UI toggle:

```python
dev = automation.LogicDeviceConfiguration(
    enabled_digital_channels=[0, 1],
    digital_sample_rate=16_000_000,
    glitch_filters=[
        automation.GlitchFilterEntry(channel_index=0, pulse_width_seconds=1e-6),
        automation.GlitchFilterEntry(channel_index=1, pulse_width_seconds=1e-6),
    ])
```

Size the pulse width **well under one bit period**: at 115200 baud a bit is **8.68 µs**, so `1e-6`
(1 µs) removes reset spikes without eating real bits. Verified to take raw-decodable frames from 0 to
clean. Note it only helps glitches — it does **not** fix a hand-decoder's framing bug (see the box
above); pair it with the Logic analyzer's own decoder.

## Capture modes

- `TimedCaptureMode(duration_seconds=...)` → `cap.wait()`.
- `ManualCaptureMode()` (the default if you omit `capture_mode`) → `cap.stop()`; `wait()` errors.
- `DigitalTriggerCaptureMode(trigger_type=..., trigger_channel_index=..., after_trigger_seconds=...)`
  → `cap.wait()` (blocks until trigger + post-trigger window). `trigger_type` is
  `automation.DigitalTriggerType.{RISING,FALLING,PULSE_HIGH,PULSE_LOW}`.

Never call both `wait()` and `stop()` on the same capture; call each at most once; neither on a
`load_capture()` result.

## Finding the device id

`manager.get_devices(include_simulation_devices=False)` → list of `DeviceDesc(device_id,
device_type, is_simulation)`. Or read it from the Logic 2 UI: capture-info sidebar → device dropdown
→ Device Info.

## Error handling

Catch `CaptureError` (and subtypes) around `start_capture`, `wait`, and `stop` — USB
bandwidth/timeout/out-of-memory failures surface there. On a `CaptureError`, discard the capture and
start a new one; don't try to save it. Other errors: `MissingDeviceError`, `DeviceError`,
`OutOfMemoryError`, `ExportError`, `LoadCaptureFailedError`, `IncompatibleApiVersionError`. The base
`SaleaeError` should not be caught directly.

## Headless (Linux/CI)

No native headless mode; run under `xvfb`:

```bash
sudo apt install xvfb libatk1.0-0 libatk-bridge2.0-0 libgtk-3-0 libgbm1
xvfb-run ./Logic-2.4.44.AppImage --automation
```

Or `automation.Manager.launch()` to start Logic 2 from the script and shut it down on `close()`.

## Gotchas

- **Omit `digital_threshold_volts` on the Logic16.** It only accepts the device's threshold *bands*
  (1.8–3.6 V / 3.6–5.0 V); a scalar like `3.3` is rejected. The default covers 3.3 V logic. Other
  devices accept a scalar.
- `add_analyzer` name + every `settings` key/value must match the Logic 2 **Add Analyzer** dialog
  exactly. SPI and Async Serial (UART) settings are given above — read other analyzers off the UI first.
- `export_raw_data_csv`/`_binary` take a `directory` that must already exist (folder, no filename);
  they emit `digital.csv`/`analog.csv` (or one `.bin` per channel).
- protobuf/grpc version clashes → `pip install --force-reinstall logic2-automation`.

See [references/reference.md](references/reference.md) for the full class/method reference.
