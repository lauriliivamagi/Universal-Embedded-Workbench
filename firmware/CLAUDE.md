# firmware/ — ESP32 DUT firmware

Context: ESP-IDF **C firmware flashed onto the ESP32 devices under test** — *not* the Pi.
Built on a laptop. Source-of-truth for the test firmware itself
(see [`../AUTHORITY.md`](../AUTHORITY.md)).

## Two projects

- `test-firmware/` — full integration firmware (UDP logging, WiFi provisioning, OTA, BLE NUS,
  captive-portal HTTP). It is the **copy-from template** referenced by the
  `workbench-integration` skill. Keep modules self-contained so they transplant cleanly.
- `debug-test/` — minimal per-chip firmware for JTAG/GDB tests. Its pre-built binaries in
  `debug-test/output/<chip>/` are consumed by the `pytest/` **WT-18xx** suite — if you change
  this firmware, **rebuild every chip's binaries**, don't hand-edit `output/`.

## Build (ESP-IDF v5.x)

```bash
cd firmware/test-firmware
idf.py set-target esp32s3      # or esp32, esp32c3, esp32c6, esp32h2
idf.py build                   # -> build/wb-test-firmware.bin
```

- Default flash 4 MB → `partitions-4mb.csv` (set in `sdkconfig.defaults`). For 8 MB+ flash use
  `partitions.csv`. See the `esp-idf-handling` skill for flash-size/partition rules.
- `CMakeLists.txt` uses relative `EXTRA_COMPONENT_DIRS` and `$ENV{IDF_PATH}` — paths are
  internal, so the `firmware/` grouping doesn't affect builds.

## Tooling (MCP)

The repo root wires up two Espressif MCP servers (see [`../.mcp.json`](../.mcp.json)) — use them
when working in this `firmware/` tree, in preference to guessing or generic web search:

- **`espressif-documentation`** — look up **ESP-IDF v5.x** APIs, Kconfig options, and guides
  (this firmware targets v5.x; ignore v6.0+ API guidance).
- **`esp-component-registry`** — search managed components before hand-rolling or running
  `idf.py add-dependency`.

They're only relevant here — the rest of the repo is Pi-side Python and docs.

## Don't

- Don't add Pi-side or host-side code here — this tree only contains firmware that runs on the
  DUT.
