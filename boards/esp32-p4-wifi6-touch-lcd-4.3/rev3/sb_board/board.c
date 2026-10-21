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
#include "bsp/touch.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lv_adapter.h"
#include "esp_pm.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "power_save.h"
static bsp_lcd_handles_t lcd;
static lv_display_t *display;
static esp_lcd_touch_handle_t touch_handle;
static atomic_bool power_enabled, display_suspended;
static atomic_uint sleep_count, sleep_ms;
static void (*power_wake)(void);
static atomic_int requested_brightness = 100;
static atomic_bool ignore_touch_until_release;
static bool power_ready;
/* A failed sleep must not create a reboot loop on the next idle timeout. */
RTC_NOINIT_ATTR static uint32_t sleep_in_progress;
#define SB_SLEEP_MARKER 0x53425057U

static esp_err_t IRAM_ATTR sleep_exit(int64_t elapsed, void *unused) {
    (void)unused;
    atomic_fetch_add(&sleep_count, 1);
    atomic_fetch_add(&sleep_ms, (uint32_t)(elapsed / 1000));
    return ESP_OK;
}
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
        if (atomic_load(&display_suspended)) {
            ESP_LOGI("board_power", "suspended: idle-sleep attempts=%u, elapsed_ms=%u",
                     atomic_load(&sleep_count), atomic_load(&sleep_ms));
#if CONFIG_PM_PROFILING
            esp_pm_dump_locks(stdout);
#endif
        } else if (age > 5000) {
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
    if (atomic_load(&ignore_touch_until_release)) {
        if (data->state == LV_INDEV_STATE_RELEASED) atomic_store(&ignore_touch_until_release, false);
        data->state = LV_INDEV_STATE_RELEASED;
    }
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
    /* Own panel/bus handles so suspend can drain LVGL before deleting DSI.
     * The BSP's start helper retains private handles and has no stop API. */
    ESP_ERROR_CHECK(esp_lv_adapter_init(&cfg.lv_adapter_cfg));
    ESP_ERROR_CHECK(bsp_display_new_with_handles(NULL, &lcd));
    esp_lv_adapter_display_config_t dc = {
        .panel = lcd.panel, .panel_io = lcd.io,
        .profile = {.interface = ESP_LV_ADAPTER_PANEL_IF_MIPI_DSI,
            .rotation = cfg.rotation, .hor_res = BSP_LCD_H_RES, .ver_res = BSP_LCD_V_RES,
            .buffer_height = 50, .use_psram = false, .enable_ppa_accel = false,
            .require_double_buffer = false},
        .tear_avoid_mode = cfg.tear_avoid_mode};
    display = esp_lv_adapter_register_display(&dc);
    ESP_ERROR_CHECK(display ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(bsp_touch_new(&cfg, &touch_handle));
    esp_lv_adapter_touch_config_t tc = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(display, touch_handle);
    lv_indev_t *touch = esp_lv_adapter_register_touch(&tc);
    ESP_ERROR_CHECK(touch ? ESP_OK : ESP_FAIL);
    ESP_ERROR_CHECK(esp_lv_adapter_start());
    bsp_display_lock(-1);
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
    requested_brightness = percent;
    if (percent == last_percent) return;
    bool blank = percent <= 0;
    if (blank != blanking.blank) {
        sb_display_blanking_set(&blanking, lv_display_get_default(), blank);
        atomic_store(&screen_blank, blank);
        ESP_LOGI("sb_display", "%s; DSI scan remains active", blank ? "screen blank" : "screen visible");
    }
    /* The power worker restores brightness only after DSI and LVGL recover. */
    esp_err_t err = bsp_display_brightness_set(atomic_load(&display_suspended) ? 0 : percent);
    if (err == ESP_OK) last_percent = percent;
    else ESP_LOGW("sb_display", "Brightness update failed: %s", esp_err_to_name(err));
}

bool sb_board_power_enabled(void) { return power_ready && atomic_load(&power_enabled); }
void sb_board_power_enable(bool enabled) { atomic_store(&power_enabled, enabled); }

static bool touch_down(void) {
    esp_lcd_touch_point_data_t point;
    uint8_t count = 0;
    if (esp_lcd_touch_read_data(touch_handle) != ESP_OK) return false;
    return esp_lcd_touch_get_data(touch_handle, &point, &count, 1) == ESP_OK && count;
}

static esp_err_t release_panel(void) {
    esp_err_t err;
    if (lcd.panel) {
        err = esp_lcd_panel_disp_on_off(lcd.panel, false);
        if (err != ESP_OK) return err;
        err = esp_lcd_panel_disp_sleep(lcd.panel, true);
        if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) return err;
        err = esp_lcd_panel_del(lcd.panel);
        if (err != ESP_OK) return err;
        lcd.panel = NULL;
    }
    if (lcd.io) {
        err = esp_lcd_panel_io_del(lcd.io);
        if (err != ESP_OK) return err;
        lcd.io = NULL;
    }
    if (lcd.mipi_dsi_bus) {
        err = esp_lcd_del_dsi_bus(lcd.mipi_dsi_bus);
        if (err != ESP_OK) return err;
        lcd.mipi_dsi_bus = NULL;
    }
    return ESP_OK;
}
static esp_err_t recover_panel(void) {
    if (!lcd.panel) {
        esp_err_t err = release_panel(); /* Finish any partially completed disposal. */
        if (err != ESP_OK) return err;
        err = bsp_display_new_with_handles(NULL, &lcd);
        if (err != ESP_OK) return err;
    } else {
        esp_err_t err = esp_lcd_panel_disp_sleep(lcd.panel, false);
        if (err != ESP_OK && err != ESP_ERR_NOT_SUPPORTED) return err;
        err = esp_lcd_panel_disp_on_off(lcd.panel, true);
        if (err != ESP_OK) return err;
    }
    return esp_lv_adapter_sleep_recover(display, lcd.panel, lcd.io);
}

static void power_worker(void *unused) {
    (void)unused;
    uint32_t blank_since = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(100));
        bool blank = atomic_load(&screen_blank);
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        if (!blank) blank_since = 0;
        else if (!blank_since) blank_since = now;
        /* Allow the black frame to finish; never pause from the LVGL worker. */
        if (!atomic_load(&display_suspended) && sb_board_power_enabled() && blank &&
            now - blank_since >= 2000) {
            atomic_store(&display_suspended, true);
            sleep_in_progress = SB_SLEEP_MARKER;
            esp_err_t err = esp_lv_adapter_sleep_prepare();
            if (err != ESP_OK) {
                atomic_store(&display_suspended, false);
                sleep_in_progress = 0;
                atomic_store(&power_enabled, false);
                ESP_LOGE("board_power", "Suspend aborted: %s; screen blanking retained", esp_err_to_name(err));
                continue;
            }
            /* sleep_prepare has waited for flushes and detached framebuffers.
             * Only now may the panel and DSI clock/CPU PM lock be released. */
            err = release_panel();
            if (err != ESP_OK) {
                atomic_store(&power_enabled, false);
                ESP_LOGE("board_power", "Panel suspend failed: %s; recovering", esp_err_to_name(err));
            }
            ESP_LOGI("board_power", "Display suspended; tickless light sleep allowed, touch polled at 100ms");
        }
        if (atomic_load(&display_suspended)) {
            /* GT911 INT is not wired on this board. Poll while LVGL is paused.
             * No manual esp_light_sleep_start(): driver PM locks must be honored. */
            bool touched = touch_down();
            if (!atomic_load(&screen_blank) || !sb_board_power_enabled() || touched) {
                if (touched) atomic_store(&ignore_touch_until_release, true);
                esp_err_t err = recover_panel();
                if (err != ESP_OK) {
                    atomic_store(&power_enabled, false);
                    ESP_LOGE("board_power", "Recovery pending: %s; retrying without reset", esp_err_to_name(err));
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    continue;
                }
                atomic_store(&last_ui_tick_ms, (uint32_t)(esp_timer_get_time() / 1000));
                atomic_store(&display_suspended, false);
                sleep_in_progress = 0;
                ESP_LOGI("board_power", "Display resumed: idle-sleep attempts=%u, elapsed_ms=%u, touch=%d",
                         atomic_load(&sleep_count), atomic_load(&sleep_ms), touched);
                if (touched && power_wake) power_wake();
                sb_display_lock();
                esp_err_t brightness_err = bsp_display_brightness_set(requested_brightness);
                if (brightness_err != ESP_OK)
                    ESP_LOGW("board_power", "Backlight restore failed: %s", esp_err_to_name(brightness_err));
                sb_display_unlock();
            }
        }
    }
}
void sb_board_power_init(bool enabled, void (*wake)(void)) {
    power_wake = wake;
    if (esp_reset_reason() != ESP_RST_POWERON && sleep_in_progress == SB_SLEEP_MARKER) {
        enabled = false;
        ESP_LOGE("board_power", "Previous sleep ended in reset; power saving disabled for this boot");
    }
    sleep_in_progress = 0;
    /* Keep the SDK's GPIO-reset workaround for unused pins, but retain live
     * SDIO/I2C/SD/sensor/BOOT/backlight levels during automatic light sleep. */
    const int active_pins[] = {14, 15, 16, 17, 18, 19, 54, 35,
        BSP_I2C_SCL, BSP_I2C_SDA, BSP_LCD_BACKLIGHT, BSP_LCD_RST,
        BSP_SD_D0, BSP_SD_D1, BSP_SD_D2, BSP_SD_D3, BSP_SD_CMD, BSP_SD_CLK,
        CONFIG_SB_NFC_SCK, CONFIG_SB_NFC_MOSI, CONFIG_SB_NFC_MISO,
        CONFIG_SB_NFC_CS, CONFIG_SB_NFC_BUSY, CONFIG_SB_NFC_RST};
    for (size_t i = 0; i < sizeof(active_pins) / sizeof(active_pins[0]); ++i)
        if (GPIO_IS_VALID_GPIO(active_pins[i]))
            ESP_ERROR_CHECK(gpio_sleep_sel_dis(active_pins[i]));
    esp_pm_config_t cfg = {.max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
                           .min_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
                           .light_sleep_enable = true};
    esp_err_t err = esp_pm_configure(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE("board_power", "Power management unavailable: %s", esp_err_to_name(err));
        return;
    }
    esp_pm_sleep_cbs_register_config_t callbacks = {.exit_cb = sleep_exit};
    ESP_ERROR_CHECK(esp_pm_light_sleep_register_cbs(&callbacks));
    atomic_store(&power_enabled, enabled);
    power_ready = xTaskCreate(power_worker, "board_power", 6144, NULL, 2, NULL) == pdPASS;
    ESP_LOGI("board_power", "Initialized: enabled=%d, worker=%d", enabled, power_ready);
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
