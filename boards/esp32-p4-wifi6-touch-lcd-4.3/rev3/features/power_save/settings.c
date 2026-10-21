// SPDX-License-Identifier: AGPL-3.0-or-later
#include "optional_power.h"
#include "../../sb_board/power_save.h"
#include "nvs.h"
#include "esp_log.h"
#include "i18n.h"
extern const lv_font_t sb_inter_14, sb_cjk_14;
void sb_power_init(void (*wake)(void)) {
    uint8_t enabled = 1;
    nvs_handle_t store;
    if (nvs_open("board_power", NVS_READONLY, &store) == ESP_OK) {
        nvs_get_u8(store, "blank_sleep", &enabled);
        nvs_close(store);
    }
    sb_board_power_init(enabled != 0, wake);
}
static void changed(lv_event_t *event) {
    bool enabled = lv_obj_has_state(lv_event_get_target_obj(event), LV_STATE_CHECKED);
    nvs_handle_t store;
    esp_err_t err = nvs_open("board_power", NVS_READWRITE, &store);
    if (err == ESP_OK) {
        err = nvs_set_u8(store, "blank_sleep", enabled);
        if (err == ESP_OK) err = nvs_commit(store);
        nvs_close(store);
    }
    if (err == ESP_OK) sb_board_power_enable(enabled);
    else {
        if (sb_board_power_enabled()) lv_obj_add_state(lv_event_get_target_obj(event), LV_STATE_CHECKED);
        else lv_obj_remove_state(lv_event_get_target_obj(event), LV_STATE_CHECKED);
        ESP_LOGE("board_power", "Cannot save sleep setting: %s", esp_err_to_name(err));
    }
}
void sb_power_settings(lv_obj_t *parent, bool chinese) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_pos(row, 390, 4);
    lv_obj_set_size(row, 355, 32);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x27272a), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 6, 0);
    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, sb_i18n_text("board.sleepAfterBlank", chinese));
    lv_obj_set_style_text_font(label, chinese ? &sb_cjk_14 : &sb_inter_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xf4f4f5), 0);
    lv_obj_set_width(label, 282);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 14, 0);
    lv_obj_t *toggle = lv_switch_create(row);
    lv_obj_set_size(toggle, 42, 24);
    lv_obj_align(toggle, LV_ALIGN_RIGHT_MID, -14, 0);
    lv_obj_set_style_bg_color(toggle, lv_color_hex(0x00ae42), LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (sb_board_power_enabled()) lv_obj_add_state(toggle, LV_STATE_CHECKED);
    lv_obj_add_event_cb(toggle, changed, LV_EVENT_VALUE_CHANGED, NULL);
}
