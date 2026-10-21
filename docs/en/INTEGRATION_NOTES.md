# Rev3.x installation and wiring

[简体中文](../INTEGRATION_NOTES.md) · [English documentation](../../README.en.md)

Supported hardware: Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3 **Rev3.x**, 32 MiB Flash, 800×480 landscape. Rev1.3 remains a historical reference and has no image in this release.

1. Flash the matching Release factory image at `0x0` for initial installation.
2. Edit `sdcard/spoolbuddy.example.json` with actual Wi-Fi credentials and Bambuddy LAN address. Save it as `spoolbuddy.json` in the TF root.
3. Boot with the card inserted. Successful backend connection confirms and saves configuration to dedicated NVS; later boots can run without the card.
4. Subsequent OTA/TF updates use the application image and manifest, not the merged factory image.

| PN5180 signal | P4 GPIO |
|---|---|
| SCK / MOSI / MISO | 2 / 3 / 4 |
| CS / BUSY / RST | 5 / 21 / 22 |

These are the board software defaults. PN5180 and the weighing module require external wiring. NAU7802 shares the BSP I²C bus supplied by `sb_board`; match wiring and module voltage to the board schematic.
