# pytest (workbench test harness)

- **Audience:** test authors
- **Deployment target:** runs on a laptop / devcontainer, against the Pi over HTTP
- **Authority:** source-of-truth for test behavior
- **Edit rule:** run from inside this directory; the driver is the API-contract surface

End-to-end tests (WT-xxx) that exercise the running workbench through its REST API. They do
**not** unit-test a Python library — they drive a real (or reachable) Pi at
`http://<host>:8080`.

```bash
pip install -r ../requirements-dev.txt
cd pytest && pytest -q                  # add --wt-url=http://pi4b.local:8080 to target a Pi
```

`workbench_test.py` imports `workbench_driver` as a **bare module**, so tests must be run
from within `pytest/` (or with it on `sys.path`). Keep this directory self-contained.

| File | Purpose |
|------|---------|
| `workbench_driver.py` | `WorkbenchDriver` — the HTTP API wrapper used by every test and by external scripts. |
| `workbench_test.py` | The WT-xxx end-to-end suite. WT-18xx flash the pre-built `firmware/debug-test/output/<chip>/` binaries. |
| `conftest.py` | Fixtures, markers (`@requires_dut`), and the `--wt-url` option. |

See [`../AUTHORITY.md`](../AUTHORITY.md).
