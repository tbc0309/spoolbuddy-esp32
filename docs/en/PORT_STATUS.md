# Feature scope · 1.2.5.7

[简体中文](../PORT_STATUS.md)

Targets Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3 Rev3.x. The LVGL interface uses Bambuddy 1.2.5.7 SpoolBuddy resources, i18n keys and APIs.

| Area | Current implementation |
|---|---|
| Pages | Home, AMS, write tag, inventory and settings |
| Data | Inventory, search, details, editing, assignment, unlinking, printers and AMS slots |
| NFC / weighing | Tag reading/writing, weight, tare and calibration; corresponding peripherals required |
| Wi-Fi / time / language | Onboard C6, backend synchronization, configuration precedence and POSIX timezone |
| Configuration | TF import, dual-bank NVS persistence, timestamps and fallback |
| Updates | HTTPS OTA, TF installation, image/board checks and boot confirmation |
| Screen | BOOT/timeout blanking, optional idle light sleep, touch or BOOT wake |

Manual weight, virtual NFC and extra confirmation are disabled by default. ESP-Hosted compatibility is a dependency adaptation; BOOT, power saving and PSRAM policy are board features.

[Board capabilities](HARDWARE_VALIDATION.md) · [Screenshots](../SCREENSHOTS.md) · [Configuration](CONFIGURATION.md) · [Release files](RELEASING.md)
