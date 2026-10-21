# Board capabilities and operation

[简体中文](../HARDWARE_VALIDATION.md)

Target: Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3 Rev3.x, 32 MiB Flash, 800×480 landscape. Compatible with Bambuddy 1.2.5.7.

| Feature | Current behavior |
|---|---|
| Display and touch | LVGL pages, bottom navigation and touch controls |
| Networking | Onboard C6 Wi-Fi; inventory, printer and AMS data from Bambuddy |
| Configuration | Initial TF import, onboard NVS persistence, previous valid configuration and boot without a card |
| Updates | HTTPS OTA and TF installation share image checks, inactive-slot writes and boot confirmation |
| Power saving | Optional connected idle light sleep for timeout and BOOT blanking; page state retained |
| Wake | Touch or a short BOOT press/release; the waking touch is consumed |
| Memory | UI and large buffers prefer PSRAM; necessary allocations retain internal memory |
| NFC / weighing | PN5180 / NAU7802 drivers and application interfaces require corresponding peripherals and wiring |

Light sleep retains active CPU frequency, PSRAM refresh and active GPIO state without entering deep sleep. AP3032 EN is tied to the supply, so software cannot disconnect backlight power. Actual consumption depends on hardware and background activity.

The simulator runs shared LVGL pages and business logic, without emulating ESP32 electronics or Flash writes. See [installation](INTEGRATION_NOTES.md) and [OTA](OTA.md).
