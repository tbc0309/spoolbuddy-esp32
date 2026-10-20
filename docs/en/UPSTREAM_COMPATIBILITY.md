# Upstream baseline and revalidation

[简体中文](../UPSTREAM_COMPATIBILITY.md) · [English documentation](../../README.en.md)

This release targets Bambuddy **1.2.5.7**. Exact commits are recorded in root `upstream-versions.json`; individual source hashes are in `upstream-map.json`. The backend contract was checked against the 1.2.5.7 OpenAPI schema. This is the tested baseline, not a claim about future upstream releases.

SpoolBuddy originates from the integrated React pages, Python daemon and FastAPI routes in maziggy/bambuddy. The ESP32 uses shared C/LVGL adaptation, runs no Linux/Pi daemon and requires no Raspberry Pi Pico. macpit reference review is kept separately in `patches/macpit`.

```powershell
python tools/check_upstream_changes.py --source upstream/bambuddy
python tools/check_backend_contract.py --expected-version 1.2.5.7
python -m pytest tests/backend/test_native_roundtrip.py -q -o asyncio_mode=auto
```

Backend tests use a disposable in-memory database without changing local inventory. Dependencies come from the pinned Bambuddy development environment. Unexecuted hardware and Linux-specific tests are not reported as ESP32 passes.
