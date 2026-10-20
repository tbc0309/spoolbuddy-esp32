// SPDX-License-Identifier: AGPL-3.0-or-later
#include "optional_sleep.h"
#include "button_state.h"
#include "board.h"
#include "ui.h"
#include "ui_live.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "lvgl.h"
static bool sleeping;
static sb_boot_button button;
static int pin;
bool sb_sleep_blank(void) { return sleeping; }
void sb_sleep_wake(void) { sleeping = false; }
static void touch(lv_event_t *e) {
    if (!sleeping) return;
    /* The first touch wakes the screen without activating the hidden control. */
    sleeping = false;
    lv_indev_t *input = (lv_indev_t *)lv_event_get_target(e);
    lv_indev_wait_release(input);
    lv_indev_stop_processing(input);
    ui_wake();
}
static void poll(lv_timer_t *timer) {
    (void)timer;
    if (sb_boot_button_poll(&button, gpio_get_level(pin) == 0, lv_tick_get())) {
        sleeping = !sleeping;
        if (!sleeping) ui_wake();
        ui_idle_tick();
        ESP_LOGI("boot_sleep", "%s", sleeping ? "screen asleep" : "screen awake");
    }
}
void sb_sleep_init(void) {
    static bool initialized;
    if (initialized) return;
    pin = sb_board_boot_gpio();
    if (pin < 0 || !GPIO_IS_VALID_GPIO(pin)) return;
    /* Input only. No hold, output drive, strap/eFuse or ROM download changes. */
    gpio_config_t cfg = {.pin_bit_mask = 1ULL << pin, .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE};
    if (gpio_config(&cfg) != ESP_OK) return;
    button.raw = gpio_get_level(pin) == 0;
    button.changed = lv_tick_get();
    lv_indev_t *input = NULL;
    while ((input = lv_indev_get_next(input)))
        if (lv_indev_get_type(input) == LV_INDEV_TYPE_POINTER)
            lv_indev_add_event_cb(input, touch, LV_EVENT_PRESSED, NULL);
    lv_timer_create(poll, 20, NULL);
    initialized = true;
    ESP_LOGI("boot_sleep", "enabled GPIO%d; release to arm", pin);
}
