"""Make the BSP's shared PHY supply acquisition idempotent for display recovery."""
from pathlib import Path
import sys

source = Path(sys.argv[1]) / "esp32_p4_wifi6_touch_lcd_4_3.c"
text = source.read_text()
original = text
replacements = [
    ("    static esp_ldo_channel_handle_t phy_pwr_chan = NULL;\n",
     "    static esp_ldo_channel_handle_t phy_pwr_chan = NULL;\n"
     "    if (phy_pwr_chan) return ESP_OK; /* SB: shared PHY supply survives display suspend */\n"),
    ("    esp_lcd_dsi_bus_handle_t mipi_dsi_bus;\n",
     "    esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL; /* SB: safe failed recovery */\n"
     "    panel_handle = NULL;\n"),
    ("    esp_lcd_panel_io_handle_t io;\n",
     "    esp_lcd_panel_io_handle_t io = NULL; /* SB: safe failed recovery */\n"),
]
for old, new in replacements:
    if new not in text:
        if text.count(old) != 1:
            raise SystemExit("Unsupported BSP display lifecycle implementation")
        text = text.replace(old, new)
if text != original:
    source.write_text(text)
