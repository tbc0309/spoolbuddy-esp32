// SPDX-License-Identifier: AGPL-3.0-or-later
#include "board.h"
#include "ui_platform.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "esp_log.h"
#include "display_blanking.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include "sdmmc_cmd.h"
static sb_display_blanking blanking;
static atomic_uint_least32_t last_ui_tick_ms;
static atomic_bool screen_blank;
static void display_tick(lv_timer_t *timer) {
    (void)timer;
    atomic_store(&last_ui_tick_ms, (uint32_t)(esp_timer_get_time() / 1000));
}
/* Independent of LVGL and its mutex. Diagnostics must still run if the UI stalls. */
static void display_health(void *unused) {
    (void)unused;
    uint32_t last_report = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        uint32_t age = now - atomic_load(&last_ui_tick_ms);
        if (age > 5000) {
            ESP_LOGE("sb_display", "LVGL progress stalled: age=%lu ms, blank=%d",
                     (unsigned long)age, atomic_load(&screen_blank));
        } else if (atomic_load(&screen_blank) && now - last_report >= 60000) {
            ESP_LOGI("sb_display", "blank health: LVGL age=%lu ms, internal=%u, PSRAM=%u",
                     (unsigned long)age,
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                     (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
            last_report = now;
        }
    }
}
static lv_indev_read_cb_t touch_read;
static void touch_read_checked(lv_indev_t *indev, lv_indev_data_t *data) {
    touch_read(indev, data);
    static bool pressed;
    bool down = data->state == LV_INDEV_STATE_PRESSED;
    if (down != pressed) {
        ESP_LOGI("sb_touch", "%s x=%ld y=%ld", down ? "down" : "up",
                 (long)data->point.x, (long)data->point.y);
        pressed = down;
    }
}
void sb_display_start(void) {
    bsp_display_cfg_t cfg = {.lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
                             .rotation = ESP_LV_ADAPTER_ROTATE_90,
                             .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_DOUBLE_FULL,
                             /* Adapter rotates pixels only. Invert physical X before swapping:
                              * logical x=physical y, logical y=480-physical x. */
                             .touch_flags = {.swap_xy = true, .mirror_x = true}};
    ESP_ERROR_CHECK(bsp_display_start_with_config(&cfg) ? ESP_OK : ESP_FAIL);
    bsp_display_lock(-1);
    lv_indev_t *touch = bsp_display_get_input_dev();
    if (touch) {
        touch_read = lv_indev_get_read_cb(touch);
        if (touch_read) lv_indev_set_read_cb(touch, touch_read_checked);
    }
    atomic_store(&last_ui_tick_ms, (uint32_t)(esp_timer_get_time() / 1000));
    lv_timer_create(display_tick, 1000, NULL);
    bsp_display_unlock();
    bsp_display_backlight_on();
    if (xTaskCreate(display_health, "display_health", 4096, NULL, 1, NULL) != pdPASS)
        ESP_LOGW("sb_display", "Display health monitor unavailable");
}
void sb_display_lock(void) {
    while (bsp_display_lock(5000) != ESP_OK)
        ESP_LOGE("sb_display", "Waiting for LVGL lock: task=%s", pcTaskGetName(NULL));
}
void sb_display_unlock(void) { bsp_display_unlock(); }
void sb_display_brightness(int percent) {
    static int last_percent = -1;
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    if (percent == last_percent) return;
    bool blank = percent <= 0;
    if (blank != blanking.blank) {
        sb_display_blanking_set(&blanking, lv_display_get_default(), blank);
        atomic_store(&screen_blank, blank);
        ESP_LOGI("sb_display", "%s; DSI scan remains active", blank ? "screen blank" : "screen visible");
    }
    esp_err_t err = bsp_display_brightness_set(percent);
    if (err == ESP_OK) last_percent = percent;
    else ESP_LOGW("sb_display", "Brightness update failed: %s", esp_err_to_name(err));
}
esp_err_t sb_board_sd_mount(void) {
    /* Reserve a sector-aligned bounce buffer before Hosted Wi-Fi consumes DMA RAM.
     * FatFs can otherwise fail even a 512-byte metadata read after networking starts.
     * The SDK serializes accesses to this card; the buffer belongs to its host. */
    static void *bounce;
    if(!bounce)bounce=heap_caps_aligned_alloc(128,4096,MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
    if(!bounce)return ESP_ERR_NO_MEM;
    esp_err_t err=bsp_sdcard_mount();
    if(err==ESP_OK && bsp_sdcard) {
        bsp_sdcard->host.dma_aligned_buffer=bounce;
        ESP_LOGI("sb_sd","Reserved 4096-byte DMA read buffer");
    } else {heap_caps_free(bounce);bounce=NULL;}
    return err;
}
const char *sb_board_sd_path(void) { return "/sdcard"; }
const char *sb_board_config_path(void) { return "/sdcard/spoolbuddy.json"; }
const char *sb_board_config_partition(void) { return "sb_config"; }
esp_err_t sb_board_i2c_init(void) { return bsp_i2c_init(); }
i2c_master_bus_handle_t sb_board_i2c_bus(void) { return bsp_i2c_get_handle(); }
const char *sb_board_name(void) { return "Waveshare P4 WiFi6 Touch LCD 4.3 Rev3.x"; }
const char *sb_board_chip(void) { return "ESP32-P4"; }
const char *sb_board_arch(void) { return "riscv32"; }
const char *sb_board_firmware_prefix(void) { return "esp32p4"; }

const char *sb_board_device_prefix(void) { return "p4"; }

spi_host_device_t sb_board_nfc_spi_host(void) { return SPI2_HOST; }
int sb_board_boot_gpio(void) { return 35; }
