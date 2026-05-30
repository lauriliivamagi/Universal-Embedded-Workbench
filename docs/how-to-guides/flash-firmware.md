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

# Flash firmware

Put a build onto a board over the network. Use `POST /api/flash` for almost
everything — it flashes on the Pi itself and works from anywhere, including
off-LAN. Drop to the RFC2217 fallback only when you have a reason to drive
`esptool` from your own machine and you're on the same LAN.

## Preferred: POST /api/flash (Pi-side esptool)

`POST /api/flash` is `multipart/form-data`. You upload the binaries; the Pi runs
`esptool write_flash` against the local USB port and manages the serial proxy for
you. The response is `{ok, output, returncode}` — `output` is esptool's combined
stdout+stderr, and `returncode: 0` means it wrote and verified.

### Form fields

| Field | Required | Default | Notes |
|---|---|---|---|
| `slot` | yes | — | Slot label, e.g. `SLOT1`. |
| `chip` | no | `auto` | `esp32`, `esp32c3`, `esp32s3`, … Let it auto-detect unless you must pin it. |
| `baud` | no | `921600` | Drop to `460800`/`115200` on flaky cables. |
| `flash_mode` | no | `dio` | |
| `flash_freq` | no | `40m` | |
| `flash_size` | no | `keep` | Keep what's on the chip, or set `4MB`, `8MB`, … |
| `erase` | no | — | `1` or `true` erases the whole flash before writing. |

Supply the binaries one of two ways.

**(a) ESP-IDF — hand it `flash_args` plus each named `.bin`.** Add a `flash_args`
part and one part per binary, each part **named by the basename `flash_args`
refers to**. From inside `build/`:

```bash
cd build
curl -s -X POST http://pi4b.local:8080/api/flash \
  -F slot=SLOT1 \
  -F chip=esp32c3 \
  -F baud=921600 \
  -F flash_args=@flash_args \
  -F bootloader.bin=@bootloader/bootloader.bin \
  -F partition-table.bin=@partition_table/partition-table.bin \
  -F my-app.bin=@my-app.bin \
  | jq '{ok, returncode}'
```

```json
{ "ok": true, "returncode": 0 }
```

**(b) Explicit offsets — name parts `bin@<offset>`.** No `flash_args` file
needed; each part is `bin@<offset>=@<file>`. This is the PlatformIO path. From
inside `.pio/build/<env>/`:

```bash
cd .pio/build/esp32c3
curl -s -X POST http://pi4b.local:8080/api/flash \
  -F slot=SLOT1 \
  -F chip=esp32c3 \
  -F 'bin@0x0000=@bootloader.bin' \
  -F 'bin@0x8000=@partitions.bin' \
  -F 'bin@0x10000=@firmware.bin' \
  | jq '{ok, returncode}'
```

**Bootloader offset depends on the chip:**

| Chip | Bootloader offset |
|---|---|
| Classic ESP32 | `0x1000` |
| ESP32-C3 / S3 / C6 / H2 | `0x0000` |

## Erase before flashing

Add `-F erase=1` to wipe the whole flash first (clears NVS, OTA data, a corrupt
partition table — a clean slate):

```bash
curl -s -X POST http://pi4b.local:8080/api/flash \
  -F slot=SLOT1 -F chip=esp32c3 -F erase=1 \
  -F flash_args=@flash_args \
  -F bootloader.bin=@bootloader/bootloader.bin \
  -F partition-table.bin=@partition_table/partition-table.bin \
  -F my-app.bin=@my-app.bin \
  | jq '{ok, returncode}'
```

## Fallback: direct esptool over RFC2217

Use this **only when your laptop is on the same LAN** as the workbench and you
need esptool running locally. Open the slot's serial port as an RFC2217 URL with
`?ign_set_control`, and **always finish with `--after no-reset`**, then issue the
reset through the API:

```bash
esptool --port 'rfc2217://pi4b.local:4001?ign_set_control' \
  --chip esp32c3 --before default-reset --after no-reset \
  write-flash --flash-mode dio --flash-size 4MB \
  0x0000 bootloader.bin 0x8000 partition-table.bin 0x10000 firmware.bin

curl -s -X POST http://pi4b.local:8080/api/serial/reset \
  -H 'Content-Type: application/json' -d '{"slot":"SLOT1"}' | jq -r '.output[]'
```

**The rule:** `--after no-reset`, then `POST /api/serial/reset`. Why `/api/flash`
is preferred: the RFC2217 `SET_CONTROL` round-trip is too slow to hold the
auto-reset window that drops the chip into the bootloader — especially off-LAN,
where the latency blows the timing entirely. Pi-side esptool toggles the lines
locally and never has that problem. See
[Serial reset and download mode](../explanation/serial-reset-and-download-mode.md).

### PlatformIO RFC2217 note

In `platformio.ini`:

```ini
upload_port = rfc2217://pi4b.local:4001
```

Append `?ign_set_control` on the command line when you invoke `pio run -t upload`.
(For most jobs, prefer the `bin@<offset>` form of `/api/flash` above.)

## Store firmware for OTA

Stash a `.bin` on the Pi so a device can pull it over the air. Upload is
`multipart/form-data` with a `project` field and a `file` part:

```bash
# Upload
curl -s -X POST http://pi4b.local:8080/api/firmware/upload \
  -F project=my-app -F file=@firmware.bin | jq '{ok, project, filename, size}'

# List what's stored
curl -s http://pi4b.local:8080/api/firmware/list \
  | jq '.files[] | {project, filename, size}'

# The device fetches it here (not under /api/):
#   http://pi4b.local:8080/firmware/my-app/firmware.bin

# Delete when done
curl -s -X DELETE http://pi4b.local:8080/api/firmware/delete \
  -H 'Content-Type: application/json' \
  -d '{"project":"my-app","filename":"firmware.bin"}' | jq .ok
```

## Gotchas / Troubleshooting

- **Part name must match the basename in `flash_args`.** If `flash_args`
  references `app.bin`, the part is `-F app.bin=@app.bin`. A mismatch fails the
  upload.
- **Flash failed?** The slot may be wedged or in a boot loop — see
  [Recover a stuck or flapping device](recover-a-stuck-or-flapping-device.md).
- **Only one RFC2217 client per slot.** The fallback path won't connect if a
  monitor session is holding the port open. Close it first.
- **Use a data cable** — charge-only cables are the #1 cause of mid-flash
  dropouts. If you run a USB hub, make sure it's externally powered; underpowered
  USB causes the same dropouts.
- **Wrong bootloader offset** is the most common silent failure on C3/S3/C6/H2 —
  it's `0x0000`, not `0x1000`.

## Related

- [Monitor output and read logs](monitor-output-and-logs.md)
- [Recover a stuck or flapping device](recover-a-stuck-or-flapping-device.md)
- [Serial reset and download mode](../explanation/serial-reset-and-download-mode.md)
- [REST API](../reference/rest-api.md)
- [Tutorial 2 — Your first flash and test](../tutorials/02-your-first-flash-and-test.md)
