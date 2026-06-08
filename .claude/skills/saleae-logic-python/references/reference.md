# Saleae Automation API (`saleae.automation`) — reference

Package `logic2-automation` (≥1.0.0), Python 3.8–3.10, Logic 2 ≥2.4.0. gRPC default port `10430`.
Wraps `saleae.proto`; raw gRPC is available for other languages. Source: `docs.saleae.com/automation`,
`llms-automation.txt`.

## Manager

`automation.Manager` — main entry point. Prefer the classmethods over `__init__`.

- `Manager.connect(*, address=127.0.0.1, port=10430, connect_timeout_seconds=None, grpc_channel_arguments=None) -> Manager`
  — connect to an already-running Logic 2.
- `Manager.launch(application_path=None, connect_timeout_seconds=None, grpc_channel_arguments=None, port=None) -> Manager`
  — launch Logic 2 and shut it down when the Manager closes.
- `get_app_info() -> AppInfo` — `{ api_version: Version(major,minor,patch), app_version, app_pid }`.
  The Python client auto-validates API version compatibility on creation.
- `get_devices(*, include_simulation_devices=False) -> List[DeviceDesc]`.
- `start_capture(*, device_configuration, device_id=None, capture_configuration=None) -> Capture`
  — all settings are explicit; UI state is ignored. `device_id=None` → first physical device.
- `load_capture(filepath) -> Capture` — returns a fully-loaded capture (no `wait()`).
- `close()` — close connection (and shut down Logic 2 if `launch()`-ed). Handled by `with`.

## Configuration types

- `LogicDeviceConfiguration(enabled_analog_channels=[], enabled_digital_channels=[],
  analog_sample_rate=None, digital_sample_rate=None, digital_threshold_volts=None, glitch_filters=[])`.
  - `GlitchFilterEntry(channel_index, pulse_width_seconds)`.
- `CaptureConfiguration(buffer_size_megabytes=None, capture_mode=ManualCaptureMode())`.
- Capture modes:
  - `TimedCaptureMode(duration_seconds, trim_data_seconds=None)` → use `wait()`.
  - `ManualCaptureMode(trim_data_seconds=None)` → use `stop()`.
  - `DigitalTriggerCaptureMode(trigger_type, trigger_channel_index, min_pulse_width_seconds=None,
    max_pulse_width_seconds=None, linked_channels=[], trim_data_seconds=None, after_trigger_seconds=None)`
    → use `wait()`.
  - `DigitalTriggerType.{RISING, FALLING, PULSE_HIGH, PULSE_LOW}`.
  - `DigitalTriggerLinkedChannel(channel_index, state)` with
    `DigitalTriggerLinkedChannelState.{LOW, HIGH}` — channel must hold that level during the event.

## Enums / descriptors

- `DeviceType.{LOGIC, LOGIC_4, LOGIC_8, LOGIC_16, LOGIC_PRO_8, LOGIC_PRO_16}`.
- `DeviceDesc(device_id, device_type, is_simulation)`.
- `RadixType.{BINARY, DECIMAL, HEXADECIMAL, ASCII}`.
- `AnalyzerHandle(analyzer_id)`; `DataTableExportConfiguration(analyzer, radix)`;
  `DataTableFilter(columns, query)`.

## Capture

Returned by `Manager.start_capture` / `load_capture`. Not user-constructable.

- `wait()` — block until a timed/trigger capture completes. Once only; not on loaded captures.
- `stop()` — stop a manual capture. Once only; never with `wait()`; not on loaded captures.
- `add_analyzer(name, *, label=None, settings=None) -> AnalyzerHandle` — `name`/`settings` must match
  the UI exactly.
- `add_high_level_analyzer(extension_directory, name, *, input_analyzer, settings=None, label=None)`
  *(out of scope for this skill — listed for completeness).*
- `remove_analyzer(handle)` / `remove_high_level_analyzer(handle)`.
- `export_data_table(filepath, analyzers, *, columns=None, filter=None, iso8601_timestamp=False)` —
  `analyzers` is a list of `AnalyzerHandle` or `DataTableExportConfiguration`.
- `export_raw_data_csv(directory, *, analog_channels=None, digital_channels=None,
  analog_downsample_ratio=1, iso8601_timestamp=False)` — directory must exist; emits
  `digital.csv`/`analog.csv`.
- `export_raw_data_binary(directory, *, analog_channels=None, digital_channels=None,
  analog_downsample_ratio=1)` — one `.bin` per channel.
- `legacy_export_analyzer(filepath, analyzer, radix)` — plugin export format; prefer
  `export_data_table`.
- `save_capture(filepath)` — `.sal`.
- `close()` — handled by `with`.

## Errors (`saleae.automation.errors`)

`SaleaeError` (base, don't catch directly) → `UnknownError`, `Logic2AlreadyRunningError`,
`IncompatibleApiVersionError`, `InternalServerError`, `InvalidRequestError`,
`LoadCaptureFailedError`, `ExportError`, `MissingDeviceError`, `CaptureError`, `DeviceError`,
`OutOfMemoryError`.

Handle `CaptureError` around `start_capture`/`wait`/`stop` and restart the capture on failure.

## Launch flags

```
Logic.exe --automation                 # Windows
./Logic/Contents/MacOS/Logic --automation   # macOS
./Logic-2.4.44.AppImage --automation   # Linux (xvfb-run for headless)
--automationPort N                     # change gRPC port (still needs --automation)
```

## Versioning

`saleae.proto` carries `major.minor.patch` (also in the `ThisApiVersion` enum). Same major =
forward/backward compatible; minor bumps add features; patch = fixes. `Manager.get_app_info()`
returns the server's version; the Python client checks compatibility automatically.
