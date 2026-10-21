# Shared implementation, platforms, boards and patches

[简体中文](../ARCHITECTURE.md) · [English documentation](../../README.en.md)

SpoolBuddy ESP32 implements an embedded client using C/LVGL and ESP-IDF. Shared application logic, platform services, board adapters and optional extensions are maintained separately.

| Location | Responsibility |
|---|---|
| `src/core` | Configuration parsing, API models/queues, calibration mathematics, statistics serialization |
| `src/ui` | Shared original-style LVGL pages and live state |
| `src/ports` | Display, board services and optional extension interfaces |
| `src/platform/esp_idf` | SDK tasks, HTTP, memory policy and real hardware statistics collection |
| `src/drivers/esp_idf` | PN5180 and NAU7802 SDK drivers |
| `boards/<model>/<revision>` | Target, private defaults, partitions, `sb_board` BSP/dependencies and board features |
| `patches` | Opt-in product extensions and separate dependency compatibility fixes |
| `simulator`, `tests`, `cmake`, `main` | Host, isolated tests, shared source selection and SDK registration |
| `vendor/spoolbuddy`, `locales` | Pinned original translations/defaults and separate local supplements |
| `upstream-map.json`, `upstream-versions.json` | Source hashes and pinned versions |

Only `esp32-p4-wifi6-touch-lcd-4.3/rev3` is currently buildable/supported. `rev1_3` is historical reference.

The board-local [screen-off power saving feature](../../boards/esp32-p4-wifi6-touch-lcd-4.3/rev3/features/power_save/README.md) connects initialization and display settings through `optional_power.h`. Sleep sequencing, touch polling and display hardware lifecycle remain private to this board.

Shared core/UI must not include BSP headers or concrete pins. SDK platform code accesses board services through `ports/board.h`. Board dependencies remain in that board's `sb_board/idf_component.yml`; only the selected board enters the build. Partitions, minimum chip revision, PSRAM and Wi-Fi coprocessor settings are board-owned.

BOOT sleep lives in the board's `features/boot_sleep`, selected through `board.cmake`; it defaults on and can be disabled with `SB_ENABLE_BOOT_SLEEP=OFF`. Shared UI calls capability interfaces, so simulator/future boards do not compile this board's GPIO handling. Board-independent manual weight, virtual NFC and additional confirmation remain opt-in patches. Original search, color catalog and calibration remain shared behavior; correctness fixes such as calibration units and integer-gram submission are not optional extensions.

## Adding a board

1. Add `boards/<model>/<revision>/` with `board.cmake`, `sdkconfig.defaults`, partitions and an `sb_board` component.
2. Implement `src/ports/board.h` and `ui_platform.h`; do not add model branches to shared pages.
3. Declare `SB_BOARD_TARGET`, `SB_BOARD_COMPONENT_DIR` and `SB_BOARD_DEFAULTS`. Keep dependencies/pins private. Optional capabilities can declare `SB_BOARD_FEATURE_SOURCES` and `SB_BOARD_FEATURE_DEFINITIONS`; unsupported capabilities retain empty implementations.
4. Use an independent build directory and sdkconfig. Reusing another board's build directory is rejected to prevent stale target/pin settings.
5. Test shared serialization, drivers, UI and backend contracts, then physically validate display, touch, network and peripherals. Do not copy the application per board.

## Statistics, configuration and updates

ESP32 statistics replace Linux probing with real CPU, temperature, memory/PSRAM, SDK, uptime and mounted-card data. Reports contain only upstream-consumed CPU/temperature, `memory`, `disk`, `os` and `system_uptime_s`. Do not fabricate `/proc`, Linux loadavg or a root filesystem from raw Flash capacity; do not add network/task/build/partition telemetry.

Configuration record formats, time and language rules are shared core behavior. NVS I/O belongs to the SDK platform; each board defines its partition. See [configuration](CONFIGURATION.md).

Manifest parsing/version comparison live in `src/core/ota_manifest`; capabilities in `src/ports/update.h`; HTTPS, Flash and boot confirmation in `src/platform/esp_idf/update_esp.c`. Each board owns its partition layout. Root `ota.json` contains release metadata, never credentials. The simulator explicitly cannot flash ESP32 firmware.

## Updating upstream and dependencies

Run `tools/check_upstream_changes.py --source upstream/bambuddy` after acquiring a new reference. It reports changed UI/API/hardware source without overwriting the port or enabling patches. Review shared changes, test them, update baseline hashes, then revalidate boards and extensions independently. React/Python to C/LVGL adaptation cannot be automatically merged across languages.

Run `tools/generate_i18n.py` after refreshing translations. Keep original snapshots, local supplements, shared widgets and board BSP separate; do not restore bilingual conditionals in widgets. Original Linux tests remain unchanged; four ESP32 replacement tests in `tests/platform/test_esp32_system_stats.py` exercise production serialization with injected snapshots, while real collector values require hardware validation.

`patches/esp_hosted_rx_retry` automatically fixes ESP-Hosted 2.13.0 SDIO retry conditions after allocation failure. It is a dependency fix, not an opt-in product extension, and changes neither SpoolBuddy resources nor GPIO. Unknown dependency source layouts stop the build; re-review when upgrading.

Rev3 Hosted PSRAM preference is private board configuration (`CONFIG_ESP_HOSTED_MEMPOOL_PREFER_SPIRAM`). Its TF sector bounce buffer remains internal DMA memory. Shared `memory_esp.c` provides PSRAM-first LVGL/cJSON and HTTP/WebSocket/configuration/OTA allocation, falling back to the default heap. Boards without PSRAM fall back normally. Rev3 additionally configures external TLS/Wi-Fi/LwIP and larger allocations, reserving 64 KiB internal memory. ISR allocations, task stacks and required DMA buffers are not forced into PSRAM.
