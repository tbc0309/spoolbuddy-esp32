"""Native Windows LVGL -> WinHTTP -> current upstream FastAPI -> disposable SQLite."""
import asyncio
import json
import shutil
import socket
from pathlib import Path

import pytest
import uvicorn


@pytest.mark.asyncio
async def test_native_roundtrip(async_client, test_engine, tmp_path, printer_factory):
    from backend.app.main import app
    from backend.app.core.config import APP_VERSION

    assert test_engine.url.database == ":memory:"
    printer = await printer_factory(name="Isolated PA printer",ip_address="127.0.0.1",is_active=False)
    response = await async_client.post("/api/v1/inventory/colors", json={
        "manufacturer":"P4 native E2E", "color_name":"Forest",
        "hex_color":"#00AE42", "material":"PLA", "effect_type":"matte"})
    assert response.status_code in (200, 201), response.text
    root = Path(__file__).resolve().parents[2]
    exe = root / "simulator/build/spoolbuddy_sim.exe"
    assert exe.exists(), "Build the native simulator first"

    async def isolation_guard():
        return {"isolated": test_engine.url.database == ":memory:", "version": APP_VERSION}

    app.add_api_route("/api/v1/port-test-isolation", isolation_guard)
    guard_route = app.routes[-1]
    # Keep the explicit test route ahead of the production SPA catch-all.
    app.router.routes.insert(0, app.router.routes.pop())
    response = await async_client.get("/api/v1/port-test-isolation")
    assert response.status_code == 200 and response.json()["isolated"] is True
    sock = socket.socket()
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    server = uvicorn.Server(uvicorn.Config(app, lifespan="off", log_level="critical", access_log=False))
    task = asyncio.create_task(server.serve(sockets=[sock]))
    config = tmp_path / "isolated.json"
    config.write_text(json.dumps({"wifi":{"ssid":"ISOLATED","password":""},
        "bambuddy":{"url":f"http://127.0.0.1:{port}","api_key":""},"language":"en"}), encoding="utf-8")
    try:
        for _ in range(100):
            if server.started:
                break
            await asyncio.sleep(.05)
        assert server.started
        process = await asyncio.create_subprocess_exec(str(exe), "--backend-test", "--config", str(config), cwd=tmp_path)
        try:
            code = await asyncio.wait_for(process.wait(), timeout=90)
        except asyncio.TimeoutError:
            process.kill()
            await process.wait()
            raise
        report = tmp_path / "port-backend-e2e.txt"
        if report.exists():
            shutil.copy2(report, root / "simulator/runtime/port-backend-e2e.txt")
        assert code == 0, report.read_text(encoding="utf-8") if report.exists() else f"exit {code}"
        # Query the real API through the upstream fixture, independently of the native model.
        response = await async_client.get("/api/v1/inventory/spools")
        assert response.status_code == 200
        spools = response.json()
        assert len(spools) == 1
        assert spools[0]["brand"] == "P4 native E2E"
        assert spools[0]["weight_used"] == pytest.approx(15)
        profiles = (await async_client.get(f"/api/v1/inventory/spools/{spools[0]['id']}/k-profiles")).json()
        assert len(profiles)==2
        assert {(p['extruder'],p['nozzle_diameter'],p['k_value']) for p in profiles}=={(0,'0.4',.021),(1,'0.6',.035)}
        assert all(p['printer_id']==printer.id for p in profiles)
        (root / "simulator/runtime/latest-backend.json").write_text(json.dumps({
            "bambuddy_version":APP_VERSION,"native_roundtrip":"PASS",
            "database":"disposable SQLite :memory:","live_inventory_modified":False,
            "hardware_tested":False,"port_version":APP_VERSION},indent=2),encoding="utf-8")
    finally:
        server.should_exit = True
        await asyncio.wait_for(task, timeout=10)
        sock.close()
        app.routes.remove(guard_route)
