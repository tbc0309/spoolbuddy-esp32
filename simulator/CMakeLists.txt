cmake_minimum_required(VERSION 3.22)
project(spoolbuddy_simulator LANGUAGES C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_NINJA_FORCE_RESPONSE_FILE ON CACHE BOOL "" FORCE)
set(LVGL_DIR "${CMAKE_CURRENT_LIST_DIR}/../managed_components/lvgl__lvgl")
if(NOT EXISTS "${LVGL_DIR}/lvgl.h")
    message(FATAL_ERROR "Resolve firmware dependencies first, or set LVGL_DIR to LVGL 9.5.0")
endif()
file(GLOB_RECURSE LVGL_SOURCES "${LVGL_DIR}/src/*.c")
option(SB_UI_PREVIEW "Build the appearance-only original layout" ON)
set(SB_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
include("${SB_ROOT}/cmake/shared.cmake")
include("${SB_ROOT}/cmake/version.cmake")
set(SB_HOST_SOURCES win32_preview.c runtime.c realtime.c realtime_test.c selftest.c backend_e2e.c stats_test.c update_stub.c)
if(SB_ENABLE_SIMULATOR_CONTROLS)
    list(APPEND SB_HOST_SOURCES "${SB_ROOT}/patches/simulator_controls/virtual_nfc.c")
endif()
add_executable(spoolbuddy_sim WIN32 ${SB_HOST_SOURCES} ${SB_SHARED_SOURCES} ${SB_PATCH_SOURCES}
    ../managed_components/espressif__cjson/cJSON/cJSON.c ${LVGL_SOURCES})
target_include_directories(spoolbuddy_sim PRIVATE . ${SB_SHARED_INCLUDES} ${SB_PATCH_INCLUDES} "${LVGL_DIR}"
    ../managed_components/espressif__cjson/cJSON)
target_compile_definitions(spoolbuddy_sim PRIVATE SB_SIMULATOR SB_FIRMWARE_VERSION="${SB_PORT_VERSION}" LV_CONF_INCLUDE_SIMPLE
    LV_LVGL_H_INCLUDE_SIMPLE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_compile_options(spoolbuddy_sim PRIVATE -O1 -fno-sanitize=undefined)
target_link_libraries(spoolbuddy_sim PRIVATE user32 gdi32 winhttp comdlg32 shell32)
target_link_options(spoolbuddy_sim PRIVATE -Wl,--subsystem,windows)

# Optional feature flags never invalidate the LVGL renderer or font object cache.
set_source_files_properties(${SB_HOST_SOURCES} "${SB_ROOT}/src/ui/ui_original.c"
    PROPERTIES COMPILE_DEFINITIONS "${SB_PATCH_DEFINITIONS}")
