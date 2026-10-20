set(SB_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
include("${SB_ROOT}/cmake/shared.cmake")
set(SB_SOURCES ${SB_SHARED_SOURCES} ${SB_PATCH_SOURCES} ${SB_BOARD_FEATURE_SOURCES}
    "${SB_ROOT}/src/platform/esp_idf/update_esp.c"
    "${SB_ROOT}/src/platform/esp_idf/memory_esp.c")
if(SB_UI_PREVIEW)
    list(APPEND SB_SOURCES "${SB_ROOT}/src/platform/esp_idf/ui_preview_main.c")
else()
    list(APPEND SB_SOURCES "${SB_ROOT}/src/platform/esp_idf/app.c" "${SB_ROOT}/src/platform/esp_idf/api.c"
        "${SB_ROOT}/src/platform/esp_idf/system_stats_esp.c" "${SB_ROOT}/src/platform/esp_idf/realtime.c"
        "${SB_ROOT}/src/platform/esp_idf/settings_store_esp.c"
        "${SB_ROOT}/src/drivers/esp_idf/pn5180.c" "${SB_ROOT}/src/drivers/esp_idf/scale.c")
endif()
idf_component_register(SRCS ${SB_SOURCES} INCLUDE_DIRS ${SB_SHARED_INCLUDES} ${SB_PATCH_INCLUDES}
    "${SB_ROOT}/src/platform/esp_idf" "${SB_ROOT}/src/drivers/esp_idf"
    REQUIRES sb_board espressif__cjson lvgl__lvgl nvs_flash esp_wifi esp_event esp_netif
    esp_http_client espressif__esp_websocket_client esp_driver_gpio esp_driver_spi esp_driver_i2c esp_driver_tsens esp_timer mbedtls fatfs spi_flash app_update)
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE
    SB_FIRMWARE_VERSION="${SB_PORT_VERSION}" SB_FIRMWARE_BUILD=${SB_FIRMWARE_BUILD}
    SB_BOARD_ID="${SB_BOARD}" ${SB_PATCH_DEFINITIONS} ${SB_BOARD_FEATURE_DEFINITIONS})
