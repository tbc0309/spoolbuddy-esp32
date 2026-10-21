# Original behavior and adaptation boundaries

[简体中文](../UPSTREAM_PARITY.md) · [English documentation](../../README.en.md)

The baseline is the Bambuddy commit in `upstream-versions.json`. Original React pages are under `upstream/bambuddy/frontend/src/pages/spoolbuddy`, with widgets in `components/spoolbuddy`. This checkout is reference-only; `vendor/spoolbuddy` contains attributable resource snapshots.

| Original entry | Shared port | Current implementation |
|---|---|---|
| Home `/spoolbuddy` | `src/ui/ui_original.c` | Tag, spool, weight, link and sync states |
| `/ams` | `ui_original.c`, `ui_ams_config.h` | AMS slots, association, configuration and PA selection |
| `/write-tag` | `ui_original.c`, `ui_form_catalog.h` | Three writing modes, forms, filament catalog and PA functions |
| `/inventory` | `ui_original.c`, `ui_inventory_detail.h` | Inventory, search, details, editing, assignment and unlinking; user names and backend error text retained |
| Settings Device/Display/Scale | `ui_original.c` | Device/display settings, current weight, offset and calibration factor; standalone calibration represented by an LVGL dialog |
| Settings Updates | `ui_updates.h`, `ports/update.h` | HTTPS OTA and TF installation replace Pi SSH updates |
| Settings System | `ui_system.h`, `core/system_stats.c` | Original web cards and fields with ESP32 statistics; no Linux loadavg or Python fields |
| Header, navigation, quick menu | `ui_original.c` | Original icons, i18n keys and clock format; quick actions follow platform capabilities |

Embedded bilingual widget strings were replaced with i18n keys. Original dictionaries and React English defaults are pinned in `vendor`; missing Chinese and platform text are supplemented only in `locales`. Known status messages follow language changes. Embedded layouts follow the original interaction structure and adapt to screen and platform capabilities.

## Following upstream updates

1. Update the pinned reference and run `tools/check_upstream_changes.py`.
2. Refresh vendor translation snapshots, review local supplements and regenerate `i18n_catalog.h`.
3. Adapt shared UI/core business behavior without duplicating it per board.
4. Compile each board independently; keep BSP, pins, display/touch, partitions and hardware features under `boards`.
5. Run native interaction, architectural boundary, configuration/update and isolated backend tests, followed by physical board validation.

Nonoriginal product extensions such as manual weight remain disabled in `patches`. NVS, timezone and OTA are platform adaptations. BOOT sleep is a board feature, separate from upstream resources.
