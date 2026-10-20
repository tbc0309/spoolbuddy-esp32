"""Enforce board and local-extension boundaries when adding future targets."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[2]


def test_bsp_includes_are_private_to_boards():
    for path in (ROOT / "src").rglob("*"):
        if path.suffix in (".c", ".h"):
            assert '#include "bsp/' not in path.read_text(encoding="utf-8"), str(path)


def test_local_patches_are_disabled_by_default_and_separate():
    manifest = json.loads((ROOT / "patches/manifest.json").read_text())
    assert manifest["default_enabled"] == []
    cmake = (ROOT / "cmake/patches.cmake").read_text()
    for patch in manifest["patches"]:
        assert patch["default"] is False
        assert (ROOT / "patches" / patch["directory"]).is_dir()
        if "option" in patch:
            assert f'option({patch["option"]} ' in cmake
            assert next(line for line in cmake.splitlines() if line.startswith(f'option({patch["option"]} ')).endswith("OFF)")


def test_rev3_has_its_own_dependencies_defaults_and_partition_table():
    board = ROOT / "boards/esp32-p4-wifi6-touch-lcd-4.3/rev3"
    for path in ("board.cmake", "sdkconfig.defaults", "partitions.csv", "sb_board/board.c", "sb_board/idf_component.yml"):
        assert (board / path).is_file()
    common = (ROOT / "sdkconfig.defaults").read_text()
    assert "CONFIG_SB_NFC_SCK=" not in common
    assert "CONFIG_ESP32P4_REV_MIN_300=" not in common
    assert "waveshare/" not in (ROOT / "main/idf_component.yml").read_text()


def test_upstream_map_covers_all_layers_and_tracks_reference_hashes():
    mapping = json.loads((ROOT / "upstream-map.json").read_text())
    for path in mapping["local_layers"].values():
        assert (ROOT / path).is_dir()
    assert {entry["area"] for entry in mapping["tracked_sources"]} == {"ui", "api", "hardware"}
    assert all(len(entry["sha256"]) == 64 for entry in mapping["tracked_sources"])


def test_boot_button_is_enabled_by_board_and_not_a_product_patch():
    board = ROOT / "boards/esp32-p4-wifi6-touch-lcd-4.3/rev3"
    assert (board / "features/boot_sleep/boot_sleep.c").is_file()
    assert 'option(SB_ENABLE_BOOT_SLEEP ' in (board / "board.cmake").read_text()
    assert 'ON)' in (board / "board.cmake").read_text()
    assert "SB_ENABLE_BOOT_SLEEP" not in (ROOT / "cmake/patches.cmake").read_text()
    assert not (ROOT / "patches/boot_sleep").exists()
    assert "SB_BOARD_FEATURE_SOURCES" in (ROOT / "main/CMakeLists.txt").read_text()
