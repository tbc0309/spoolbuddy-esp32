// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ui.h"
#include "ui_live.h"
#include "ui_original.h"
#include "ui_platform.h"
#include "optional_sleep.h"
#include "optional_power.h"
#include "settings.h"
#include "i18n.h"
#include "lvgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
sb_live_t sb_live = {.brightness = 100};
void ui_init(void) {
    sb_live.enabled = true;
    original_init(sb_settings.chinese);
    sb_sleep_init();
    sb_power_init(ui_wake);
}
void ui_status(const char *text) {
    sb_display_lock();
    original_status(text);
    sb_display_unlock();
}
void ui_tag(const char *uid, const char *text) {
    (void)text;
    sb_display_lock();
    snprintf(sb_live.uid, sizeof(sb_live.uid), "%s", uid ? uid : "");
    sb_live.spool_id = 0;
    sb_live.tray_uuid[0] = 0;
    original_live_changed();
    sb_display_unlock();
}
void ui_details(const char *uid, const char *text) {
    sb_display_lock();
    if (!strcmp(sb_live.uid, uid))
        original_status(text);
    sb_display_unlock();
}
void ui_spool(const char *uid, int id, const char *brand, const char *material, float remaining,
              float core) {
    sb_display_lock();
    if (!strcmp(sb_live.uid, uid)) {
        sb_live.spool_id = id;
        sb_live.core = core;
        sb_live.remaining = remaining;
        snprintf(sb_live.brand, sizeof(sb_live.brand), "%s", brand);
        snprintf(sb_live.material, sizeof(sb_live.material), "%s", material);
        original_live_changed();
    }
    sb_display_unlock();
}
void ui_weight(float grams, bool stable, bool valid) {
    sb_display_lock();
    sb_live.sensor = grams;
    sb_live.stable = stable;
    sb_live.valid = valid && isfinite(grams) && grams >= 0 && grams <= 100000;
    original_live_changed();
    sb_display_unlock();
}
bool ui_take_tare(void) {
    sb_display_lock();
    bool value = sb_live.tare_pending;
    sb_live.tare_pending = false;
    sb_display_unlock();
    return value;
}
void ui_display_settings(int percent, int seconds) {
    sb_display_lock();
    sb_live.brightness = percent < 0 ? 0 : percent > 100 ? 100 : percent;
    sb_live.blank_seconds = seconds < 0 ? 0 : seconds;
    ui_idle_tick();
    sb_display_unlock();
}
void ui_local_brightness(int percent) { ui_display_settings(percent, sb_live.blank_seconds); }
int ui_brightness_value(void) { return sb_live.brightness; }
void ui_wake(void) {
    sb_display_lock();
    sb_sleep_wake();
    lv_display_trigger_activity(NULL);
    sb_display_brightness(sb_live.brightness);
    sb_display_unlock();
}
void ui_language(bool chinese) {
    sb_settings.chinese = chinese;
    original_language(chinese);
}
void ui_raw(int32_t raw, bool valid) {
    sb_display_lock();
    sb_live.raw = raw;
    sb_live.raw_valid = valid;
    sb_display_unlock();
}
void ui_idle_tick(void) {
    bool blank = sb_live.blank_seconds > 0 &&
                 lv_display_get_inactive_time(NULL) >= (uint32_t)sb_live.blank_seconds * 1000;
    sb_display_brightness((blank || sb_sleep_blank()) ? 0 : sb_live.brightness);
}

void ui_tag_uuid(const char *uid, const char *uuid) {
    sb_display_lock();
    if (!strcmp(sb_live.uid, uid))
        snprintf(sb_live.tray_uuid, sizeof(sb_live.tray_uuid), "%s", uuid ? uuid : "");
    sb_display_unlock();
}
