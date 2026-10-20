// SPDX-License-Identifier: AGPL-3.0-or-later
#include "system_stats_esp.h"
#include "system_stats.h"
#include "board.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include <stdio.h>
#include "soc/soc_caps.h"
#if SOC_TEMP_SENSOR_SUPPORTED
#include "driver/temperature_sensor.h"
#endif
cJSON *sb_system_stats_collect(void) {
    float temperature = 0;
    bool temperature_valid = false;
#if SOC_TEMP_SENSOR_SUPPORTED
    static temperature_sensor_handle_t sensor;
    static bool attempted;
    if (!attempted) {
        attempted = true;
        temperature_sensor_config_t cfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(0, 80);
        if (temperature_sensor_install(&cfg, &sensor) == ESP_OK &&
            temperature_sensor_enable(sensor) != ESP_OK) {
            temperature_sensor_uninstall(sensor);
            sensor = NULL;
        }
    }
    if (sensor) temperature_valid = temperature_sensor_get_celsius(sensor, &temperature) == ESP_OK;
#endif
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    uint64_t sd_total = 0, sd_free = 0;
    bool sd_available = esp_vfs_fat_info(sb_board_sd_path(), &sd_total, &sd_free) == ESP_OK;
    char kernel[48];snprintf(kernel,sizeof(kernel),"FreeRTOS %s",tskKERNEL_VERSION_NUMBER);
    sb_system_snapshot_t s = {
        .kernel = kernel,

        .arch = sb_board_arch(),
        .idf_version = esp_get_idf_version(),
        .cores = chip.cores,

        .uptime_us = (uint64_t)esp_timer_get_time(),

        .heap_total = heap_caps_get_total_size(MALLOC_CAP_8BIT),
        .heap_free = heap_caps_get_free_size(MALLOC_CAP_8BIT),

        .sd_available = sd_available,
        .temperature_valid = temperature_valid,
        .temperature_c = temperature,
        .sd_total = sd_total,
        .sd_free = sd_free};
    return sb_system_stats_build(&s);
}
