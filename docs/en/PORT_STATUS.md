# Port status: 1.2.5.7

[简体中文](../PORT_STATUS.md) · [English documentation](../../README.en.md)

Release target: Waveshare ESP32-P4-WIFI6-Touch-LCD-4.3 Rev3.x. The port follows Bambuddy 1.2.5.7's integrated SpoolBuddy resources, i18n keys and APIs. LVGL reconstructs the UI; exact pixel equivalence for every dynamic state is not claimed.

| Area | Software validation | Hardware validation |
|---|---|---|
| Home, AMS, write tag, inventory, settings | Production widget interaction; 28 bilingual page states | Display, touch and navigation passed |
| Inventory, printers and AMS data | Original API and isolated backend integration | Registration, heartbeats and two inventory spools displayed |
| NFC, weighing, tare, calibration, writing | Business logic, driver compilation, requests and failure paths | PN5180/NAU7802 not yet wired and validated |
| Wi-Fi, time, language | Backend synchronization, configuration precedence, timezone | Onboard C6 connection and short-duration heartbeat test passed |
| Initial TF configuration and NVS backup | Timestamps, dual-bank persistence and fallback | Boot without a card passed after confirmation |
| TF firmware update | Image, size, hash, board and version checks | Installation, restart and VALID boot confirmation passed |
| HTTPS OTA | Shared installer, TLS, manifest and credential isolation | Complete download/install and intentional rollback pending |
| BOOT sleep and touch wake | Separate board feature; ROM download retained | Hardware validation confirmed full black and successful touch wake |

Manual weight, virtual NFC and additional confirmation are disabled by default. The ESP-Hosted compatibility fix is a required dependency fix. BOOT handling and PSRAM configuration belong to this board.
