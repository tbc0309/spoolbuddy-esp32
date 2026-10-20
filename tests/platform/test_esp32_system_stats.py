"""Four ESP32 replacements for the inapplicable Linux daemon stats tests.

The real serializer is compiled into the EXE; hardware acquisition awaits a board.
"""
import json
import subprocess
from pathlib import Path

import pytest


@pytest.fixture(scope="module")
def stats(tmp_path_factory):
    root = Path(__file__).resolve().parents[2]
    work = tmp_path_factory.mktemp("esp32-stats")
    subprocess.run([str(root / "simulator/build/spoolbuddy_sim.exe"), "--stats-test"], cwd=work, check=True, timeout=20)
    return json.loads((work / "esp32-stats-fixtures.json").read_text(encoding="utf-8"), parse_constant=lambda value: pytest.fail(value))


def test_esp32_storage_replaces_linux_root_disk(stats):
    disk = stats["normal"]["disk"]
    assert disk == {"total_gb":16, "free_gb":12, "used_gb":4, "percent":25}
    assert "disk" not in stats["no_sd"]
    assert "hardware" not in stats["normal"]


def test_esp32_chip_replaces_linux_load_average(stats):
    assert stats["normal"]["cpu_count"] == 2
    assert stats["normal"]["os"]["arch"] == "riscv32"
    assert "load_avg" not in stats["normal"]  # FreeRTOS does not provide Linux loadavg.


def test_esp32_collects_heap_psram_platform_and_uptime(stats):
    normal = stats["normal"]
    assert normal["memory"]["total_mb"] == 8
    assert normal["memory"]["available_mb"] == 2
    assert normal["memory"]["used_mb"] == 6
    assert normal["memory"]["percent"] == 75
    assert set(normal["memory"]) == {"total_mb","available_mb","used_mb","percent"}
    assert normal["os"]["kernel"] == "FreeRTOS"
    assert normal["os"]["arch"] == "riscv32"
    assert normal["system_uptime_s"] == 123


def test_esp32_stats_json_bounds_and_unavailable_hardware(stats):
    assert stats["hardware_probed"] is False
    for key in ("normal", "no_sd", "edge"):
        assert len(json.dumps(stats[key], allow_nan=False).encode()) < 4096
    assert stats["edge"]["memory"]["percent"] == 0
    assert stats["edge"]["memory"]["used_mb"] == 0
    assert set(stats["normal"]) == {"os","cpu_count","system_uptime_s","memory","disk"}
    assert set(stats["normal"]["os"]) == {"os","kernel","arch"}
    assert "disk" not in stats["edge"]
