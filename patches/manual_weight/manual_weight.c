// SPDX-License-Identifier: AGPL-3.0-or-later
#include "manual_weight.h"
#include "ui.h"
#include "ui_live.h"
#include "ui_platform.h"
#include "settings.h"
#include <math.h>
#include <stdlib.h>
#include "bridge.h"
static bool manual_active, manual_pending;
static float manual_grams;
bool ui_manual_active(void) {
    sb_display_lock();
    bool value = manual_active;
    sb_display_unlock();
    return value;
}
bool ui_take_manual(float *grams) {
    sb_display_lock();
    bool value = manual_pending;
    *grams = manual_grams;
    manual_pending = false;
    sb_display_unlock();
    return value;
}
bool ui_manual_value(float *grams) {
    sb_display_lock();
    bool value = manual_active;
    *grams = manual_grams;
    sb_display_unlock();
    return value;
}

void ui_manual_set(float grams) {
    if (!isfinite(grams) || grams < 0 || grams > 100000)
        return;
    sb_display_lock();
    manual_active = true;
    manual_pending = true;
    manual_grams = grams;
    original_live_changed();
    sb_display_unlock();
}
void ui_manual_clear(void) {
    sb_display_lock();
    manual_active = false;
    manual_pending = false;
    original_live_changed();
    sb_display_unlock();
}

typedef struct {
    lv_obj_t *input, *keyboard;
    void (*close)(void);
    bool chinese;
} dialog_t;
static void dialog_deleted(lv_event_t *e) {
    dialog_t *d = lv_event_get_user_data(e);
    if (d->keyboard)
        lv_obj_delete(d->keyboard);
    free(d);
}
static void key_done(lv_event_t *e) {
    dialog_t *d = lv_event_get_user_data(e);
    lv_obj_t *k = d->keyboard;
    d->keyboard = NULL;
    if (k)
        lv_obj_delete(k);
}
static void input_focus(lv_event_t *e) {
    dialog_t *d = lv_event_get_user_data(e);
    if (d->keyboard)
        return;
    d->keyboard = lv_keyboard_create(lv_screen_active());
    lv_obj_set_pos(d->keyboard, 0, 240);
    lv_obj_set_size(d->keyboard, 800, 240);
    lv_keyboard_set_mode(d->keyboard, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(d->keyboard, d->input);
    lv_obj_add_event_cb(d->keyboard, key_done, LV_EVENT_READY, d);
    lv_obj_add_event_cb(d->keyboard, key_done, LV_EVENT_CANCEL, d);
}
static void apply(lv_event_t *e) {
    dialog_t *d = lv_event_get_user_data(e);
    float grams;
    if (!sb_parse_weight(lv_textarea_get_text(d->input), &grams)) {
        original_status(d->chinese ? "请输入 0 至 100000 克" : "Enter 0-100000 grams");
        return;
    }
    ui_manual_set(grams);
    void (*close)(void) = d->close;
    close();
    original_model_changed();
}
static void clear(lv_event_t *e) {
    dialog_t *d = lv_event_get_user_data(e);
    ui_manual_clear();
    void (*close)(void) = d->close;
    close();
    original_model_changed();
}
static void make_button(lv_obj_t *parent, const char *caption, int x, int y, int width,
                        lv_event_cb_t cb, dialog_t *d) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, width, 44);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x00ae42), 0);
    lv_obj_t *label = lv_label_create(b);
    lv_label_set_text(label, caption);
    lv_obj_center(label);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, d);
}
void sb_manual_weight_open(lv_obj_t *panel, const lv_font_t *font, bool chinese,
                           void (*close)(void)) {
    dialog_t *d = calloc(1, sizeof(*d));
    if (!d) {
        close();
        return;
    }
    d->close = close;
    d->chinese = chinese;
    lv_obj_set_style_text_font(panel, font, 0);
    lv_obj_add_event_cb(panel, dialog_deleted, LV_EVENT_DELETE, d);
    lv_obj_t *help = lv_label_create(panel);
    lv_label_set_text(help,
                      chinese ? "请输入耗材加空盘的总重。" : "Include filament and empty spool.");
    lv_obj_set_pos(help, 20, 58);
    d->input = lv_textarea_create(panel);
    lv_textarea_set_one_line(d->input, true);
    lv_obj_set_pos(d->input, 20, 90);
    lv_obj_set_size(d->input, 232, 48);
    lv_textarea_set_accepted_chars(d->input, "0123456789.");
    lv_obj_set_user_data(d->input, (void *)"manual_weight");
    lv_obj_add_event_cb(d->input, input_focus, LV_EVENT_FOCUSED, d);
    make_button(panel, chinese ? "采用重量" : "Apply", 270, 90, 230, apply, d);
    make_button(panel, chinese ? "切换到电子秤" : "Use electronic scale", 20, 156, 480, clear, d);
}

#include "optional_inputs.h"
void sb_optional_inputs_poll(int64_t now_us, sb_optional_weight_sink sink, void *context) {
    static int64_t next_report;
    float grams;
    bool changed = ui_take_manual(&grams);
    if (sink && ui_manual_value(&grams) && (changed || now_us >= next_report)) {
        sink(grams, context);
        next_report = now_us + 1000000;
    }
}
