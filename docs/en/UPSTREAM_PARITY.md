# Original behavior and adaptation boundaries

[简体中文](../UPSTREAM_PARITY.md) · [English documentation](../../README.en.md)

The baseline is the Bambuddy commit in `upstream-versions.json`. Original React pages are under `upstream/bambuddy/frontend/src/pages/spoolbuddy`, with widgets in `components/spoolbuddy`. This checkout is reference-only; `vendor/spoolbuddy` contains attributable resource snapshots.

| Original entry | Shared port | Validation and remaining differences |
|---|---|---|
| Home `/spoolbuddy` | `src/ui/ui_original.c` | Native callbacks cover tag, spool, weight, link and sync states; physical NFC/weighing pending |
| `/ams` | `ui_original.c`, `ui_ams_config.h` | Original slot, association, configuration and PA APIs; isolated regression coverage; physical printer actions not fully validated |
| `/write-tag` | `ui_original.c`, `ui_form_catalog.h` | Three modes, forms, catalogs and PA covered by simulator and isolated real-backend tests; actual tag writing pending |
| `/inventory` | `ui_original.c`, `ui_inventory_detail.h` | Inventory, search, detail, editing, assignment and unlink regressions; user-defined names/backend errors are not forcibly translated |
| Settings Device/Display/Scale | `ui_original.c` | Display and calibration interfaces connected, including current weight, offset and factor. Original standalone calibration is represented by an LVGL dialog; layout is not pixel-identical |
| Settings Updates | `ui_updates.h`, `ports/update.h` | Project HTTPS OTA and TF installation replace Pi SSH updates; physical install/rollback evidence tracked separately |
| Settings System | `ui_system.h`, `core/system_stats.c` | Original web cards/fields; FreeRTOS does not fabricate Linux loadavg or Python information |
| Header, navigation, quick menu | `ui_original.c` | Original icons, translation keys and clock format; Linux/browser operations handled by platform capability |

Embedded bilingual widget strings were replaced with i18n keys. Original dictionaries and React English defaults are pinned in `vendor`; missing Chinese and platform text are supplemented only in `locales`. Known status messages follow language changes. This matrix does not claim complete physical testing or universal pixel equivalence.

## Following upstream updates

1. Update the pinned reference and run `tools/check_upstream_changes.py`.
2. Refresh vendor translation snapshots, review local supplements and regenerate `i18n_catalog.h`.
3. Adapt shared UI/core business behavior without duplicating it per board.
4. Compile each board independently; keep BSP, pins, display/touch, partitions and hardware features under `boards`.
5. Run native interaction, architectural boundary, configuration/update and isolated backend tests, followed by physical board validation.

Nonoriginal product extensions such as manual weight remain disabled in `patches`. NVS, timezone and OTA are platform adaptations. BOOT sleep is a board feature, separate from upstream resources.
