---
type: how-to-guide
domain: complicated
audience: operator
stability: structural
authority:
  provenance: institutional
  verifiability: executable
  evidence: strong
  currency: dated
epistemic-layer: method
---

# Maintain and update the workbench

Keep the workbench healthy: manage the portal service, deploy code updates, add a
new Pi model, and apply config changes. This is the **one** area where SSH is
appropriate — for deploying code and managing the service. Day-to-day board
operation always goes through the HTTP API; never SSH in to drive a board.

## Manage the service (over SSH)

```bash
sudo systemctl status rfc2217-portal       # is it running?
sudo systemctl restart rfc2217-portal      # restart (also clears stale slot state)
sudo systemctl start rfc2217-portal        # start
sudo systemctl stop rfc2217-portal         # stop
sudo journalctl -u rfc2217-portal -f       # follow logs live
```

A restart also clears stale slot state, which is often all a wedged slot needs.

## Update all scripts

Pull the repo on the Pi, then run the installer in update mode:

```bash
cd pi && sudo bash install.sh --update
```

`--update` re-copies the portal, controllers, and config to `/usr/local/bin`,
**skips** apt/pip/openocd installation and service-masking, and restarts the
portal.

## Quick single-file deploy

From your dev machine, push one file and restart (replace `pi4b.local` with
your `SERIAL_PI` IP if you use a fixed address):

```bash
# portal.py -> the running portal
scp pi/portal.py pi4b@pi4b.local:/tmp/portal.py && \
  ssh pi4b@pi4b.local 'sudo cp /tmp/portal.py /usr/local/bin/rfc2217-portal && sudo systemctl restart rfc2217-portal'

# debug_controller.py -> its installed copy
scp pi/debug_controller.py pi4b@pi4b.local:/tmp/debug_controller.py && \
  ssh pi4b@pi4b.local 'sudo cp /tmp/debug_controller.py /usr/local/bin/debug_controller.py && sudo systemctl restart rfc2217-portal'
```

## SSH is only for deploy and service tasks

To be unambiguous: **SSH is only for the deploy and service-management tasks on
this page.** Everything you do to *operate* a board — flash, monitor, reset,
recover, GPIO, WiFi, BLE, debug — goes through the HTTP API at
`http://pi4b.local:8080`. Never SSH in to drive a board.

## Fix a wrong slot count (phantom-port filter)

If auto-detection reports the wrong number of slots, the portal has a per-model
**phantom-port filter** keyed on `/proc/device-tree/model` (some Pi USB hubs
advertise ports that aren't wired to a physical jack). To tune it:

1. Plug a board into **every** physical USB port (and every hub port, if you
   added a hub).
2. Watch the portal's auto-detection log line over journalctl:

   ```bash
   sudo journalctl -u rfc2217-portal -f
   # look for:  [portal] auto-detected N USB hub port(s): [...]
   ```

3. Compare the listed port prefixes against the jacks that are actually occupied.
4. Add the **unwired** (phantom) prefixes to your model's phantom-port list in
   `pi/portal.py`.
5. Redeploy `portal.py` (see [Quick single-file deploy](#quick-single-file-deploy))
   and restart.

This is a code change followed by a deploy.

## Change config

Edit the config on the Pi, then restart the service:

```bash
sudo nano /etc/rfc2217/workbench.json      # slots, GPIO pins, debug probes
sudo nano /etc/rfc2217/signalgen.json      # signal-generator wiring
sudo systemctl restart rfc2217-portal
```

See [Configuration files](../reference/configuration-files.md) for the schemas.

## Gotchas

- **After any change under `/usr/local/bin`, you must restart the service** — the
  portal does not hot-reload.
- **A stuck slot is often cleared by a service restart** — but try
  `POST /api/serial/recover` first; see
  [Recover a stuck or flapping device](recover-a-stuck-or-flapping-device.md).
- **Tuning the phantom-port filter is a code edit**, not a config edit — it lives
  in `pi/portal.py` and ships via deploy.

## Related

- [Configuration files](../reference/configuration-files.md)
- [Slot states and network ports](../reference/slot-states-and-network-ports.md)
- [Architecture overview](../explanation/architecture-overview.md)
- [Recover a stuck or flapping device](recover-a-stuck-or-flapping-device.md)
