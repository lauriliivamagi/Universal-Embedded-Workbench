# pytest/ — workbench end-to-end test harness

Context: tests that drive a **running workbench over its HTTP API** (not a Python library's
unit tests). Source-of-truth for test behavior (see [`../AUTHORITY.md`](../AUTHORITY.md)).

## Run from inside this directory

`workbench_test.py` imports `workbench_driver` as a **bare module**, so always run from
`pytest/` (or put it on `sys.path`):

```bash
pip install -r ../requirements-dev.txt
cd pytest
pytest -q --wt-url http://pi4b.local:8080     # target a real Pi
pytest -q --run-dut                            # also run @requires_dut tests
```

- `--wt-url` defaults to `$WORKBENCH_URL` or `http://localhost:8080`.
- DUT-dependent tests are marked `@requires_dut` and are **skipped unless `--run-dut`**.

## Conventions

- **Never SSH the Pi.** All interaction goes through `WorkbenchDriver` (the API-contract
  surface) — `workbench`/`wifi_network` fixtures live in `conftest.py`.
- Test IDs are `WT-xxx`. **WT-18xx are order-dependent**: `WT-1800` (the flash) must pass
  first, and they flash the pre-built `../firmware/debug-test/output/<chip>/` binaries.
- When you add an API endpoint in `pi/`, extend `workbench_driver.py` and add a `WT-xxx` test.
