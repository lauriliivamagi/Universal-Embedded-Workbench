# Logic 2 MCP — tool reference

Server: `http://127.0.0.1:10530`, Streamable HTTP transport, server name `saleae-logic2`
(v2.4.44 verified). No auth/session id required. 15 tools.

Argument schemas below are extracted live from the running server. `*` = required.
Channel selections always nest under `logicChannels` (`digitalChannels` / `analogChannels` arrays).

## Device & capture lifecycle

### `get_devices`
- `includeSimulationDevices`: boolean — include demo devices.
- Returns `{"devices":[{deviceId, deviceType, isSimulation}]}`.

### `start_capture`
- `logicDeviceConfiguration`* : object
  - `logicChannels`: `{ digitalChannels: number[], analogChannels: number[] }`
  - `digitalSampleRate`: number (S/s, e.g. `16000000`)
  - `analogSampleRate`: number
  - `digitalThresholdVolts`: number — **omit on Logic16** (see SKILL.md gotchas)
  - `glitchFilters`: array
- `captureConfiguration` : object — provide exactly one mode (defaults to manual if omitted)
  - `bufferSizeMegabytes`: number
  - `manualCaptureMode`: `{ trimDataSeconds? }`
  - `timedCaptureMode`: `{ durationSeconds*, trimDataSeconds? }`
  - `digitalCaptureMode`: `{ triggerType, triggerChannelIndex, afterTriggerSeconds?, minPulseWidthSeconds?, maxPulseWidthSeconds?, trimDataSeconds?, linkedChannels? }`
- `deviceId`: string — omit to use the first physical (non-sim) device.
- Returns `{"captureId": <number>}`.

`triggerType` enum (numeric): `0`=rising, `1`=falling, `2`=pulse-high, `3`=pulse-low.

### `wait_capture`
- `captureId`* : number. Blocks until a timed/trigger capture finishes. Do not use with manual mode.

### `stop_capture`
- `captureId`* : number. For manual mode. Never combine with `wait_capture` on one capture.

### `save_capture`
- `captureId`* , `filepath`* (.sal). Lowercase `filepath`.

### `load_capture`
- `filepath`* (.sal). Returns a loaded capture (already complete; no wait needed).

### `close_capture`
- `captureId`* . Always call when done — frees resources.

## Analyzers

### `add_analyzer`
- `captureId`* , `analyzerName`* (must match the Logic 2 Add-Analyzer list exactly).
- `analyzerLabel`: string. `settings`: object — keys/values must match the UI exactly.
- Returns an analyzer id. Add only after the capture has data.

### `remove_analyzer`
- `captureId`* , `analyzerId`* .

### `add_high_level_analyzer` *(out of scope for this skill — listed for completeness)*
- `captureId`* , `extensionDirectory`* , `hlaName`* , `inputAnalyzerId`* , `hlaLabel?`, `settings?`.

### `remove_high_level_analyzer`
- `captureId`* , `analyzerId`* .

## Export

### `export_data_table_csv`
- `captureId`* , `filepath`* .
- `analyzers`: array of analyzer ids to include.
- `exportColumns`: array of column names. `iso8601Timestamp`: boolean (wall-clock vs capture-relative).
- `filter`: `{ query*, columns? }`.

### `export_raw_data_csv`
- `captureId`* , `directory`* (must exist; folder only), `analogDownsampleRatio`* (use `1`).
- `logicChannels`: `{ digitalChannels?, analogChannels? }` — omit to export all.
- `iso8601Timestamp`: boolean. Produces `digital.csv` / `analog.csv`.

### `export_raw_data_binary`
- Same as CSV minus `iso8601Timestamp`. Produces one `.bin` per channel.
- Format: https://support.saleae.com/faq/technical-faq/binary-export-format-logic-2

### `legacy_export_analyzer`
- `captureId`* , `filepath`* , `analyzerId`* , `radixType`* (numeric radix). Plugin export format,
  not the data table. Prefer `export_data_table_csv`.

## Notes

- Source corpus: `docs.saleae.com/mcp`, `llms-mcp.txt`. Live schema snapshot:
  `.firecrawl/saleae/mcp-tools-live.json`.
- MCP support is experimental; `digitalCaptureMode`/HLA shapes may evolve between Logic 2 releases —
  re-run `tools/list` (via the MCP client) if a call's schema looks different.
