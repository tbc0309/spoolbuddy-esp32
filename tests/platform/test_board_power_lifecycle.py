"""Check that the BSP lifecycle adaptation remains repeatable and fails closed."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "boards/esp32-p4-wifi6-touch-lcd-4.3/rev3/features/power_save/apply_bsp.py"


def test_bsp_recovery_adaptation_is_idempotent(tmp_path):
    source = tmp_path / "esp32_p4_wifi6_touch_lcd_4_3.c"
    source.write_text(
        "    static esp_ldo_channel_handle_t phy_pwr_chan = NULL;\n"
        "    esp_lcd_dsi_bus_handle_t mipi_dsi_bus;\n"
        "    esp_lcd_panel_io_handle_t io;\n"
    )
    subprocess.run([sys.executable, str(SCRIPT), str(tmp_path)], check=True)
    once = source.read_bytes()
    subprocess.run([sys.executable, str(SCRIPT), str(tmp_path)], check=True)
    assert source.read_bytes() == once
    assert b"if (phy_pwr_chan) return ESP_OK" in once
    assert b"panel_handle = NULL" in once


def test_unknown_bsp_revision_is_not_partially_modified(tmp_path):
    source = tmp_path / "esp32_p4_wifi6_touch_lcd_4_3.c"
    original = b"/* An unsupported upstream display implementation */\n"
    source.write_bytes(original)
    result = subprocess.run([sys.executable, str(SCRIPT), str(tmp_path)], capture_output=True)
    assert result.returncode != 0
    assert source.read_bytes() == original
