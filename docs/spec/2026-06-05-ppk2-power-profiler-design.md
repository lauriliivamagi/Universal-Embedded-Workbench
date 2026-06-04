# PPK2 Power Profiler — Integration Design

**Status:** Design intent (non-authoritative per [`../../AUTHORITY.md`](../../AUTHORITY.md)).
Once built, behavior is documented code-first in `pi/` + `docs/`; this file records the
design decisions, not shipped behavior.

**Date:** 2026-06-05
**Topic:** Integrate a Nordic Power Profiler Kit II (PPK2) into the workbench as a
scriptable, computer-controllable instrument behind the REST API.

## Goal

Expose the PPK2 over the workbench HTTP API as a control wrapper, mirroring the existing
signal-generator pattern: an agent or test script can select a measurement mode, set the
source voltage, toggle DUT power, and read aggregated current — without SSHing to the Pi or
touching the Nordic desktop app.

Decisions taken during brainstorming:

- **Outcome:** scriptable control wrapper (not test-phase correlation or a portal dashboard).
- **Modes:** both Source Meter and Ampere Meter, selectable per session via a `mode` param.
- **Readout:** both rolling-stats poll and fixed-window capture, from one sampler.
- **Sampler architecture:** in-portal background thread (Approach A), matching how every other
  instrument (siggen/wifi/ble) is wired.
- **Voltage cap:** configurable in `power.json`; default = hardware max (5000 mV). Operators
  tighten it to protect lower-voltage DUTs.
- **Spec location:** `docs/spec/`.

## Non-goals (YAGNI / future)

- Test-phase / serial-log time correlation of current traces.
- Live current telemetry streamed to the web portal dashboard.
- PPK2 digital logic inputs (D0–D7) event capture.

None of these require reworking this wrapper; they layer on top later.

## Architecture

### Placement & dependency

- New module `pi/power_controller.py` exposing a `PowerController` class that wraps the
  `ppk2-api` PyPI library (the community Nordic PPK2 serial driver).
- `ppk2-api` added to `pi/install.sh` dependency install and to `requirements*.txt`.
- Instantiated once in `pi/portal.py`, with the same graceful-disable fallback as the signal
  generator (`portal.py:95-101`): if the library import fails or no PPK2 is present,
  `_power = None` and every `/api/power/*` endpoint returns a clean "unavailable" response
  via a `_power_unavailable()` guard.
- Optional config `/etc/rfc2217/power.json`, installed from `pi/config/power.json`. All fields
  optional with sensible defaults:

  | Field | Type | Default | Meaning |
  | --- | --- | --- | --- |
  | `serial_port` | string | auto-detect | Override PPK2 serial device path |
  | `default_mode` | `"source"` \| `"ampere"` | `"source"` | Mode if `start` omits it |
  | `default_voltage_mv` | int | `3300` | Source voltage if `start` omits it |
  | `voltage_cap_mv` | int | `5000` | Reject `start`/voltage above this (configurable safety limit) |
  | `capture_ring_seconds` | number | `30` | Bounded raw-sample ring used for CSV capture |

### Device binding & slot exclusion (required portal change)

The PPK2 enumerates as a CDC-ACM serial device (VID:PID `1915:c00a`). Today
`_port_is_serial_usable()` (`portal.py:324`) keeps any `cdc_acm` device as a slot, so the
portal would expose the PPK2 as a fake slot and run RFC2217 + esptool chip-detection against
it.

Change: in `_port_is_serial_usable()`, read `idVendor`/`idProduct` from the sysfs child device
and return `False` for `1915:c00a` (a small VID:PID skip-list beside the existing driver
filter). The port is then never allocated as a slot. `PowerController` discovers the PPK2 by
scanning for that same VID:PID (or honors `serial_port` from config).

Single instance, one session at a time, guarded by a lock.

## API surface (`/api/power/*`)

Dispatched in `portal.py` exactly like the `/api/siggen/*` handlers, each backed by a
`_handle_power_*` method behind the `_power_unavailable()` guard. Every response carries the
standard `{"ok": true|false, ...}` envelope.

| Method | Endpoint | Body | Returns |
| --- | --- | --- | --- |
| GET | `/api/power/status` | — | `{present, active, mode, source_voltage_mv, dut_power, hardware:{ppk2, serial_port}}` |
| POST | `/api/power/start` | `{mode?, voltage_mv?, dut_power?, force?}` | starts session + sampler → `{ok, mode, source_voltage_mv, dut_power}` |
| POST | `/api/power/stop` | — | stops sampler, powers off DUT (source mode), closes. Idempotent |
| GET | `/api/power/measure` | — | rolling stats since last poll: `{mean_ua, min_ua, max_ua, last_ua, samples, window_ms, charge_uc}` |
| POST | `/api/power/capture` | `{duration_ms, csv?}` | timed window: `{mean_ua, min_ua, max_ua, charge_uc, samples, csv?}` (`csv` path returned only when `csv:true` requested) |
| POST | `/api/power/dut` | `{on}` | toggle DUT power output (source mode only), session stays live |

Validation:

- `mode` ∈ `{"source", "ampere"}`; defaults to `default_mode`.
- `voltage_mv` is required-or-defaulted in source mode, **ignored** in ampere mode. Range
  `[800, 5000]` AND `≤ voltage_cap_mv`, else `400`.
- `dut`/`voltage` operations in ampere mode → `400` (no power control when externally powered).
- Second `start` while `active` → `409` unless `force:true` (which stops then restarts).

CSV captures are written to a captures directory and the **path** is returned, never the raw
array — same "return a path, not megabytes" principle the scope MCP uses.

## Sampler internals (Approach A)

On `start()`: open the PPK2, `use_source_meter()`/`use_ampere_meter()`, set source voltage,
set DUT power per `dut_power` (default OFF), `start_measuring()`, then spawn a daemon thread.

Thread loop: read the stream, convert to a buffer of per-sample microamp values
(`get_samples()`), and fold them under a lock into a rolling accumulator:

- `count`, `sum_ua`, `min_ua`, `max_ua`, `last_ua`
- `charge_uc += mean_ua · dt` (dt from the PPK2 sample period)
- a bounded ring buffer of the most recent `capture_ring_seconds` of samples (for CSV)

Reads:

- `measure()` — snapshot the accumulator, compute mean/min/max/last + `window_ms` + `charge_uc`
  since the previous `measure()`, then reset the rolling fields. Stats are "since last poll."
- `capture(duration_ms)` — reset, sleep `duration_ms`, snapshot; if a CSV is requested, dump
  the ring buffer for the window. If `duration_ms` exceeds the ring capacity, **`log()` the
  truncation** (no silent cap) and return stats over the full window with a truncated CSV.

Raw 100 ksps data is aggregated on the Pi and never shipped over HTTP.

## Safety guardrails

Borrowed from the scope MCP's guardrail ethos (typed, range-checked, no foot-guns):

- Source voltage clamped to `[800, 5000]` mV and rejected above `voltage_cap_mv` (configurable;
  default 5000 = hardware max). Operators set a lower cap in `power.json` to protect 3.3 V DUTs.
- DUT power defaults **OFF** on `start` unless `dut_power:true`.
- On `stop()`, portal shutdown, or PPK2 disconnect: stop sampling, power off the DUT, release
  the serial device. The controller registers a shutdown hook so a portal restart never leaves
  the DUT powered.
- One session at a time; concurrent `start` is rejected unless `force`.

## Consumers

### WorkbenchDriver (`pytest/workbench_driver.py`)

Mirror the API one-for-one:

```python
wt.power_status()
wt.power_start(mode="source", voltage_mv=3300, dut_power=True)
wt.power_measure()                  # rolling stats since last poll
wt.power_capture(duration_ms=5000)  # timed window (+ optional csv path)
wt.power_dut(on=False)              # power-cycle without ending the session
wt.power_stop()
```

### Skill (`.claude/skills/workbench-power/`)

Modeled on the `signal-generator` skill:

- "Always GET `/api/power/status` first" — confirm `hardware.ppk2`, pick a mode.
- Mode guidance: source vs ampere, when each applies, the voltage-cap behavior.
- Both readout patterns (poll loop vs timed capture) with recipes.
- Safety notes (DUT defaults off; voltage cap; release on stop).
- "Don't sleep blindly" — poll the DUT/serial for the expected state rather than fixed delays.

### Documentation

Behavior documented code-first: a "Power Profiler (PPK2)" service section in `docs/`, plus an
entry in the README service list and the API reference table. Per AUTHORITY, the FSD is not
the place for shipped behavior.

## Testing

- **Unit (no hardware):** inject a stub sampler that yields synthetic microamp buffers.
  Verify accumulator math (mean/min/max/charge), `measure()` reset semantics, mode guards,
  voltage range + cap rejection, ampere-mode power-control rejection, `force` restart.
- **Integration (hardware-gated, like the other workbench tests):** pytest against the live
  Pi `/api/power/*` — start source mode at 3300 mV, `measure`, `capture`, toggle `dut`, `stop`.
  Skipped when no PPK2 is present.

## Open questions

None outstanding. Voltage-cap default and spec location resolved during brainstorming.
