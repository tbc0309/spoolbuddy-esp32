set(SB_SHARED_SOURCES
    "${SB_ROOT}/src/ui/ui_original.c" "${SB_ROOT}/src/ui/ui_live.c"
    "${SB_ROOT}/src/core/bridge.c" "${SB_ROOT}/src/core/settings.c"
    "${SB_ROOT}/src/core/config_record.c"
    "${SB_ROOT}/src/core/system_stats.c" "${SB_ROOT}/src/core/ota_manifest.c" "${SB_ROOT}/src/core/i18n.c" "${SB_ROOT}/src/core/clock.c"
    "${SB_ROOT}/assets/fonts/sb_inter_12.c" "${SB_ROOT}/assets/fonts/sb_inter_14.c"
    "${SB_ROOT}/assets/fonts/sb_inter_20.c" "${SB_ROOT}/assets/fonts/sb_cjk_14.c"
    "${SB_ROOT}/assets/fonts/sb_cjk_18.c" "${SB_ROOT}/assets/spoolbuddy_logo.c"
    "${SB_ROOT}/assets/ui_icons.c")
set(SB_SHARED_INCLUDES "${SB_ROOT}/src/core" "${SB_ROOT}/src/ui" "${SB_ROOT}/src/ports")
include("${SB_ROOT}/cmake/patches.cmake")
