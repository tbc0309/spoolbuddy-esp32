# Validation scope

[简体中文](../HARDWARE_VALIDATION.md)

Platform: Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3 Rev3.x, 32 MiB Flash; Bambuddy 1.2.5.7. Recorded on 2026-10-20.

| Feature | Status |
|---|---|
| Display, touch and navigation | Passed on hardware |
| BOOT sleep and touch wake | Passed; power consumption not measured |
| TF configuration, NVS and boot without a card | Passed; timestamps, dual-bank persistence and fallback also tested in software |
| TF firmware installation, reboot and boot confirmation | Passed on hardware |
| Wi-Fi, registration, heartbeats and inventory reads | Passed on hardware |
| PSRAM and system statistics | Allocation probe and backend field checks passed |
| Adaptive update layout | Native bilingual coverage for idle, checking, available version, download, verification, reboot, error and no update |
| PN5180 / NAU7802 | Physical wiring validation pending |
| HTTPS OTA / intentional rollback | Complete hardware end-to-end validation pending |

Short-duration network testing covered five minutes after reboot; all 120 pings of 1400 bytes returned. This does not establish hours-long stability. Release `release-manifest.json` identifies the image SHA-256, SDK and tested scope.

Automated tests use isolated data. Simulator screenshots support layout review; they do not certify every sensor, electrical behavior or real printer action.
