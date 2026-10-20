// SPDX-License-Identifier: AGPL-3.0-or-later
#include "board.h"
#include "ui_platform.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"
#include "sdmmc_cmd.h"
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
    bsp_display_unlock();
    bsp_display_backlight_on();
}
void sb_display_lock(void) { bsp_display_lock(-1); }
void sb_display_unlock(void) { bsp_display_unlock(); }
void sb_display_brightness(int percent) {
    static bool panel_blanked;
    bool blank = percent <= 0;
    if (blank != panel_blanked) {
        /* BSP backlight_off is only a PWM change. Explicitly blank LCD pixels too.
         * The AP3032 EN is pulled up on this board, not connected to a host GPIO. */
        esp_err_t err = esp_lcd_panel_disp_on_off(bsp_display_get_panel_handle(), !blank);
        if (err == ESP_OK) panel_blanked = blank;
        ESP_LOGI("sb_display", "panel %s: %s", blank ? "off" : "on", esp_err_to_name(err));
    }
    bsp_display_brightness_set(percent);
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
