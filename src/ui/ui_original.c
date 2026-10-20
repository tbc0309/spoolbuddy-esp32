// SPDX-License-Identifier: AGPL-3.0-or-later
// Native LVGL port of the upstream SpoolBuddy kiosk, at 800 x 480.
#include "ui_original.h"
#include "ui.h"
#include "ui_platform.h"
#include "lvgl.h"
#include "bridge.h"
#include "ui_live.h"
#include "settings.h"
#include "i18n.h"
#include "update.h"
#ifdef SB_ENABLE_ACTION_CONFIRMATION
#include "optional_ui.h"
#endif
#ifdef SB_ENABLE_MANUAL_WEIGHT
#include "optional_ui.h"
#endif
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
extern const lv_font_t sb_inter_12, sb_inter_14, sb_inter_20, sb_cjk_14, sb_cjk_18;
extern const lv_image_dsc_t sb_logo;
extern const lv_image_dsc_t sb_nav_0_gray, sb_nav_1_gray, sb_nav_2_gray, sb_nav_3_gray,
    sb_nav_4_gray;
extern const lv_image_dsc_t sb_nav_0_green, sb_nav_1_green, sb_nav_2_green, sb_nav_3_green,
    sb_nav_4_green;
extern const lv_image_dsc_t sb_icon_offline, sb_icon_wifi, sb_icon_search, sb_icon_scale,
    sb_icon_tag, sb_icon_info, sb_icon_chevron;
extern const lv_image_dsc_t sb_icon_wifioff, sb_icon_signal;
static const lv_image_dsc_t *nav_gray[] = {&sb_nav_0_gray, &sb_nav_1_gray, &sb_nav_2_gray,
                                           &sb_nav_3_gray, &sb_nav_4_gray};
static const lv_image_dsc_t *nav_green[] = {&sb_nav_0_green, &sb_nav_1_green, &sb_nav_2_green,
                                            &sb_nav_3_green, &sb_nav_4_green};
static const unsigned BG = 0x181c20, SECONDARY = 0x262c32, BORDER = 0x363e46, GREEN = 0x00ae42,
                      TEXT = 0xd4d4d8, MUTED = 0x71717a, CARD = 0x27272a;
static lv_font_t regular, small, heading, cjk, cjk_heading;
static lv_obj_t *body, *navs[5], *nav_labels[5], *nav_icons[5], *status, *led, *clock_label,
    *overlay, *keyboard;
static int current_page, write_tab, settings_tab, filter, blank_option;
static bool chinese, full_form, pa_profile;
static bool preview_online, quick_add;
static lv_obj_t *presence, *presence_dot, *network_icon;
static lv_obj_t *settings_content;
static lv_obj_t *settings_weight, *settings_stability;
static bool network_connected;
void ui_network(bool connected) {
    sb_display_lock();
    if(network_connected==connected){sb_display_unlock();return;}
    network_connected=connected;
    if(network_icon)lv_image_set_src(network_icon,connected?&sb_icon_signal:&sb_icon_wifioff);
    sb_display_unlock();
}
static lv_obj_t *printer_select, *scale_readout, *tag_readout, *manual_input;
static char feedback[256], search_query[96], write_query[96], material_filter[64];
static char feedback_key[128];
static char material_options[32][64];
static int material_count;
static lv_obj_t *write_list, *write_panel;
static void write_preview(void);
static bool feedback_error;
static bool contains(const char *haystack, const char *needle);
static int selected_spool, selected_slot;
static cJSON *draft;
static int picker_mode;
static lv_obj_t *inventory_grid;
static bool rendering;
static lv_obj_t *diagnostic_output;
static void diagnostic_show(void);
static void color_picker(void);
static void color_choose(lv_event_t *e);
static lv_obj_t *catalog_list;
static char catalog_query[96];
static void catalog_render(void);
static void preset_open(lv_event_t *e);
static void preset_render(void);
static void pa_render(void);
static void quick_render(void);
static void pa_auto(lv_event_t *e);
static void original_confirm(const char *title, const char *method, const char *path,
                             const cJSON *payload);
static lv_obj_t *preset_list, *pa_list, *quick_list;
static char preset_query[96];
static cJSON *selected_profiles, *create_profiles;
static bool create_waiting;
static char create_namespace[64];
static int create_quantity = 1;
static char create_path[96], result_note[192];
static char quick_tag_uid[33];
static bool contains(const char *haystack, const char *needle);
static bool core_touched;
static lv_obj_t *quick_swatches[13];
static const char *quick_names[] = {"Black", "White",  "Gray", "Red",   "Orange", "Yellow", "Green",
                                    "Blue",  "Purple", "Pink", "Brown", "Silver", "Clear"};
static const char *quick_names_cn[] = {"黑色", "白色", "灰色", "红色", "橙色", "黄色", "绿色",
                                       "蓝色", "紫色", "粉色", "棕色", "银色", "透明"};
static const unsigned quick_colors[] = {0x000000, 0xffffff, 0x808080, 0xff0000, 0xffa500,
                                        0xffff00, 0x00ae42, 0x0066ff, 0x8b00ff, 0xff69b4,
                                        0x8b4513, 0xc0c0c0, 0};
static void quick_color_search(lv_event_t *e) {
    const char *query = lv_textarea_get_text(lv_event_get_target_obj(e));
    for (int i = 0; i < 13; i++)
        if (quick_swatches[i]) {
            char hex[9];
            snprintf(hex, sizeof(hex), "%06X", quick_colors[i]);
            if (contains(quick_names[i], query) || contains(quick_names_cn[i], query) ||
                contains(hex, query))
                lv_obj_remove_flag(quick_swatches[i], LV_OBJ_FLAG_HIDDEN);
            else
                lv_obj_add_flag(quick_swatches[i], LV_OBJ_FLAG_HIDDEN);
        }
}
static lv_obj_t *location_select;
static void location_choices(void);
static void location_changed(lv_event_t *e);
static void core_catalog_open(lv_event_t *e);
static bool quick_tag_create;
static void unknown_add_dialog(void);
static cJSON *slot_draft;
static bool preset_for_slot;
static int config_slot, config_printer;
static void ams_slot_actions(int code);
static lv_obj_t *slot_link_list;
static void slot_link_render(void);
static void ams_config_show(void);
static void slot_preset_selected(const cJSON *preset, int source);
static void ams_command_result(const char *path, int code, const cJSON *reply);
static const cJSON *prop(const cJSON *o, const char *key) {
    return cJSON_GetObjectItemCaseSensitive(o, key);
}
static const char *str(const cJSON *o, const char *key) {
    const cJSON *v = prop(o, key);
    return cJSON_IsString(v) ? v->valuestring : "";
}
static double number(const cJSON *o, const char *key, double fallback) {
    const cJSON *v = prop(o, key);
    return cJSON_IsNumber(v) ? v->valuedouble : fallback;
}
static const cJSON *spool_by_id(int id) {
    const cJSON *s;
    cJSON_ArrayForEach(s, bridge_model("inventory")) if ((int)number(s, "id", 0) == id) return s;
    return NULL;
}
static unsigned spool_color(const cJSON *s) {
    const char *hex = str(s, "rgba");
    return strlen(hex) == 8 ? (unsigned)strtoul(hex, NULL, 16) >> 8 : 0x808080;
}
static float gross(void) {
    float grams;
    return ui_manual_value(&grams) ? grams : sb_live.sensor;
}
static bool valid_weight(void) { return ui_manual_active() || sb_live.valid; }
static void detail_show(int id);
static void detail_values(void);
static lv_obj_t *detail_remaining, *detail_scale, *detail_profiles;
static int detail_spool;
static void action_cb(lv_event_t *e);
static void inventory_detail(lv_event_t *e);
static void draft_reset(void);
static void draft_bind(lv_obj_t *field, const char *key);
static cJSON *draft_payload(void);
static const cJSON *own_device(void) {
    const cJSON *d;
    cJSON_ArrayForEach(
        d, bridge_model("devices")) if (!strcmp(str(d, "device_id"), bridge_device())) return d;
    return NULL;
}
static void display_save(int brightness, int seconds) {
    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(p, "brightness", brightness);
    cJSON_AddNumberToObject(p, "blank_timeout", seconds);
    char path[192];
    snprintf(path, sizeof(path), "/spoolbuddy/devices/%s/display", bridge_device());
    bridge_submit("PUT", path, p);
    cJSON_Delete(p);
}
static void brightness_changed(lv_event_t *e) {
    if (sb_live.enabled) {
        int value = lv_slider_get_value(lv_event_get_target_obj(e));
        ui_local_brightness(value);
        display_save(value, sb_live.blank_seconds);
    }
}
static void dropdown_draft(lv_event_t *e) {
    if (!sb_live.enabled)
        return;
    char value[64];
    lv_dropdown_get_selected_str(lv_event_get_target_obj(e), value, sizeof(value));
    if (!draft)
        draft_reset();
    const char *key = lv_event_get_user_data(e);
    cJSON_DeleteItemFromObjectCaseSensitive(draft, key);
    if (!strcmp(key, "effect_type")) {
        if (!strcmp(value, "None"))
            return;
        for (char *p = value; *p; p++)
            *p = *p == ' ' ? '-' : (char)tolower((unsigned char)*p);
    }
    cJSON_AddStringToObject(draft, key, value);
}
enum {
    ACT_WRITE = 1,
    ACT_CANCEL_WRITE,
    ACT_SYNC,
    ACT_ASSIGN,
    ACT_TARE,
    ACT_CALIBRATE,
    ACT_CREATE,
    ACT_LINK,
    ACT_UNLINK,
    ACT_REBOOT,
    ACT_SHUTDOWN,
    ACT_RESTART,
    ACT_DIAG_NFC,
    ACT_DIAG_SCALE,
    ACT_DIAG_TAG,
    ACT_CHECK_UPDATE,
    ACT_UPDATE_SD,
    ACT_SAVE_CONFIG,
    ACT_RESTART_BROWSER,
    ACT_UNKNOWN_ADD
};
static const char *tr(const char *key) { return sb_i18n_text(key, chinese); }
static void draw_page(void);
static void keyboard_close(lv_event_t *e);
static void style(lv_obj_t *o, unsigned bg, int radius) {
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_shadow_width(o, 0, 0);
    lv_obj_set_style_text_font(o, &regular, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(TEXT), 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}
static lv_obj_t *rect(lv_obj_t *p, int x, int y, int w, int h, unsigned bg, int radius) {
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    style(o, bg, radius);
    return o;
}
static lv_obj_t *text(lv_obj_t *p, const char *s, int x, int y, int w, unsigned color,
                      const lv_font_t *font) {
    lv_obj_t *o = lv_label_create(p);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_width(o, w);
    lv_obj_set_style_text_font(o, font ? font : &regular, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_text_line_space(o, 3, 0);
    lv_label_set_text(o, s);
    return o;
}
static lv_obj_t *center_text(lv_obj_t *p, const char *s, int x, int y, int w, unsigned color,
                             const lv_font_t *font) {
    lv_obj_t *o = text(p, s, x, y, w, color, font);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    return o;
}
static lv_obj_t *image(lv_obj_t *p, const lv_image_dsc_t *src, int x, int y) {
    lv_obj_t *o = lv_image_create(p);
    lv_image_set_src(o, src);
    lv_obj_set_pos(o, x, y);
    return o;
}
static lv_obj_t *circle(lv_obj_t *p, int x, int y, int size, unsigned color) {
    return rect(p, x, y, size, size, color, LV_RADIUS_CIRCLE);
}
static void border(lv_obj_t *o, unsigned color, int width) {
    lv_obj_set_style_border_width(o, width, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(color), 0);
}
static lv_obj_t *button(lv_obj_t *p, const char *caption, int x, int y, int w, int h, unsigned bg,
                        lv_event_cb_t cb, int value) {
    lv_obj_t *o = lv_button_create(p);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    style(o, bg, 6);
    lv_obj_t *t = lv_label_create(o);
    lv_label_set_text(t, caption);
    lv_obj_set_style_text_font(t, &regular, 0);
    lv_obj_center(t);
    if (cb)
        lv_obj_add_event_cb(o, cb, LV_EVENT_CLICKED, (void *)(intptr_t)value);
    return o;
}
static void noop(lv_event_t *e) {
    (void)e;
    if (sb_live.enabled)
        original_status(
            tr("port.ThisControlNeedsASupportedBackendCapability"));
}
static void close_overlay(lv_event_t *e) {
    (void)e;
    keyboard_close(NULL);
    if (overlay)
        lv_obj_delete(overlay);
    overlay = NULL;
    diagnostic_output = manual_input = catalog_list = preset_list = quick_list = NULL;
    detail_remaining = detail_scale = detail_profiles = slot_link_list = NULL;
}
static void field_focus(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_FOCUSED)
        return;
    if (keyboard)
        lv_obj_delete(keyboard);
    keyboard = lv_keyboard_create(lv_screen_active());
    lv_obj_set_pos(keyboard, 0, 240);
    lv_obj_set_size(keyboard, 800, 240);
    lv_obj_set_style_text_font(keyboard, &regular, LV_PART_ITEMS);
    lv_keyboard_set_textarea(keyboard, lv_event_get_target_obj(e));
    if (lv_event_get_target_obj(e) == manual_input)
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_add_event_cb(keyboard, keyboard_close, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(keyboard, keyboard_close, LV_EVENT_CANCEL, NULL);
}
static void keyboard_close(lv_event_t *e) {
    (void)e;
    if (keyboard)
        lv_obj_delete(keyboard);
    keyboard = NULL;
}
static lv_obj_t *field(lv_obj_t *p, const char *placeholder, int x, int y, int w, int h) {
    lv_obj_t *o = lv_textarea_create(p);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    style(o, 0x363e46, 3);
    lv_obj_set_style_pad_left(o, 12, 0);
    lv_obj_set_style_pad_top(o, 7, 0);
    lv_textarea_set_one_line(o, true);
    lv_textarea_set_placeholder_text(o, placeholder);
    lv_textarea_set_max_length(o, 80);
    lv_obj_set_height(o, h);
    lv_obj_add_event_cb(o, field_focus, LV_EVENT_FOCUSED, NULL);
    return o;
}
static void spool(lv_obj_t *p, int x, int y, int size, unsigned color) {
    circle(p, x + size / 28, y + size / 28, size - size / 14, color);
    unsigned r = ((color >> 16) & 255) * 85 / 100, g = ((color >> 8) & 255) * 85 / 100,
             b = (color & 255) * 85 / 100;
    circle(p, x + size / 7, y + size / 7, size * 5 / 7, (r << 16) | (g << 8) | b);
    lv_obj_t *shine = circle(p, x + size * 15 / 56, y + size * 16 / 56, size * 9 / 56, 0xffffff);
    lv_obj_set_style_bg_opa(shine, 70, 0);
    circle(p, x + size * 20 / 56, y + size * 20 / 56, size * 16 / 56, 0x2d2d2d);
    circle(p, x + size * 23 / 56, y + size * 23 / 56, size * 10 / 56, 0x1a1a1a);
}
static bool updating_printers;
static void printer_changed(lv_event_t *e) {
    if (!sb_live.enabled || updating_printers)
        return;
    int index = lv_dropdown_get_selected(lv_event_get_target_obj(e));
    const cJSON *p = cJSON_GetArrayItem(bridge_model("printers"), index);
    if (p) {
        bridge_printer((int)number(p, "id", 0));
        original_model_changed();
    }
}
static void nav_cb(lv_event_t *e) { original_page((int)(intptr_t)lv_event_get_user_data(e)); }
static void write_cb(lv_event_t *e) {
    write_tab = (int)(intptr_t)lv_event_get_user_data(e);
    draw_page();
}
static void settings_cb(lv_event_t *e) {
    settings_tab = (int)(intptr_t)lv_event_get_user_data(e);
    draw_page();
}
static void filter_cb(lv_event_t *e) {
    filter = (int)(intptr_t)lv_event_get_user_data(e);
    if (sb_live.enabled)
        snprintf(material_filter, sizeof(material_filter), "%s",
                 filter >= 2 && filter - 2 < material_count ? material_options[filter - 2] : "");
    draw_page();
}
static void view_cb(lv_event_t *e) {
    full_form = (int)(intptr_t)lv_event_get_user_data(e);
    if (full_form && sb_live.enabled)
        bridge_load_catalogs();
    draw_page();
}
static void quick_add_changed(lv_event_t *e) {
    quick_add = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
    if (quick_add) {
        pa_profile = false;
        cJSON_Delete(selected_profiles);
        selected_profiles = cJSON_CreateArray();
    }
    draw_page();
}
static void profile_cb(lv_event_t *e) {
    pa_profile = (int)(intptr_t)lv_event_get_user_data(e);
    draw_page();
}
static void blank_cb(lv_event_t *e) {
    blank_option = (int)(intptr_t)lv_event_get_user_data(e);
    if (sb_live.enabled) {
        int seconds[] = {0, 60, 120, 300, 600, 1800};
        ui_display_settings(sb_live.brightness, seconds[blank_option]);
        display_save(sb_live.brightness, seconds[blank_option]);
    }
    draw_page();
}
static void quick_menu(lv_event_t *e) {
    (void)e;
    if (overlay)
        return;
    overlay = rect(lv_screen_active(), 0, 0, 800, 480, 0x000000, 0);
    lv_obj_set_style_bg_opa(overlay, 128, 0);
    lv_obj_add_event_cb(overlay, close_overlay, LV_EVENT_CLICKED, NULL);
    lv_obj_t *panel = rect(overlay, 0, 0, 800, sb_live.enabled ? 390 : 217, SECONDARY, 14);
    rect(panel, 382, 8, 36, 4, 0x52525b, 4);
    text(panel, tr("port.System"), 14, 32, 720, MUTED, &small);
    const char *en[] = {tr("spoolbuddy.quickMenu.restartDaemon"), tr("spoolbuddy.quickMenu.restartBrowser"), tr("spoolbuddy.quickMenu.reboot"), tr("spoolbuddy.quickMenu.shutdown")};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *b = button(panel, en[i], 14 + (i % 2) * 392, 58 + (i / 2) * 51,
                             380, 43, CARD, action_cb,
                             i == 2   ? ACT_REBOOT
                             : i == 3 ? ACT_SHUTDOWN
                             : i == 1 ? ACT_RESTART_BROWSER
                                      : ACT_RESTART);
        lv_obj_set_style_text_color(b,
                                    lv_color_hex(i == 2   ? 0x9b7d42
                                                 : i == 3 ? 0x915354
                                                          : MUTED),
                                    0);
    }
    if (sb_live.enabled) {
        text(panel, tr("port.PrinterPower"), 14, 176, 720, MUTED, &small);
        quick_list = rect(panel, 14, 202, 772, 152, SECONDARY, 0);
        lv_obj_add_flag(quick_list, LV_OBJ_FLAG_SCROLLABLE);
        bridge_lookup("plugs", "/smart-plugs/");
        quick_render();
    }
    center_text(panel, tr("port.TapOutsideToClose"), 0, sb_live.enabled ? 366 : 176,
                800, 0x52525b, &small);
}
static void dashboard(void) {
    lv_obj_t *stats = rect(body, 14, 14, 772, 38, 0x202428, 8);
    border(stats, 0x2d343b, 1);
    text(stats, "2", 14, 8, 30, 0xf4f4f5, &heading);
    text(stats, tr("spoolbuddy.inventory.spools"), 36, 13, 62, MUTED, &small);
    rect(stats, 94, 10, 1, 18, BORDER, 0);
    text(stats, "2", 116, 8, 28, 0xf4f4f5, &heading);
    text(stats, tr("spoolbuddy.spool.material"), 140, 13, 66, MUTED, &small);
    rect(stats, 206, 10, 1, 18, BORDER, 0);
    text(stats, "1", 228, 8, 24, 0xf4f4f5, &heading);
    if (sb_live.enabled) {
        int n = cJSON_GetArraySize(bridge_model("inventory"));
        int materials = 0, brands = 0;
        const cJSON *s, *previous;
        cJSON_ArrayForEach(s, bridge_model("inventory")) {
            bool m = true, b = true;
            cJSON_ArrayForEach(previous, bridge_model("inventory")) {
                if (previous == s)
                    break;
                if (!strcmp(str(previous, "material"), str(s, "material")))
                    m = false;
                if (!strcmp(str(previous, "brand"), str(s, "brand")))
                    b = false;
            }
            materials += m;
            brands += b;
        }
        char value[24];
        int counts[] = {n, materials, brands}, positions[] = {14, 116, 228};
        for (int i = 0; i < 3; i++) {
            snprintf(value, sizeof(value), "%d", counts[i]);
            lv_obj_t *old = lv_obj_get_child(stats, i == 0 ? 0 : i == 1 ? 3 : 6);
            lv_label_set_text(old, value);
            (void)positions;
        }
    }
    text(stats, tr("spoolbuddy.spool.brand"), 251, 13, 70, MUTED, &small);
    lv_obj_t *device = rect(body, 14, 63, 316, 184, BG, 10);
    border(device, 0x2b3137, 1);
    text(device, tr("port.Device"), 14, 14, 280, 0xa1a1aa, &small);
    circle(device, 14, 50, 8, preview_online ? GREEN : 0xef4444);
    text(device, preview_online ? tr("spoolbuddy.status.online") : tr("spoolbuddy.status.offline"), 33, 45, 255,
         0x9ca3af, &regular);
    lv_obj_t *scale = rect(device, 14, 75, 288, 46, 0x202428, 8);
    image(scale, &sb_icon_scale, 12, 15);
    text(scale, tr("spoolbuddy.settings.scale"), 34, 15, 170, 0x71717a, &small);
    scale_readout = text(scale, "—", 190, 12, 88, 0xd4d4d8, &regular);
    lv_obj_set_style_text_align(scale_readout, LV_TEXT_ALIGN_RIGHT, 0);
#ifdef SB_ENABLE_MANUAL_WEIGHT
    if (sb_live.enabled)
        lv_obj_add_event_cb(scale, action_cb, LV_EVENT_CLICKED, (void *)(intptr_t)0);
#endif
    lv_obj_t *nfc = rect(device, 14, 130, 288, 39, 0x202428, 8);
    image(nfc, &sb_icon_tag, 12, 12);
    text(nfc, "NFC", 34, 12, 120, 0x71717a, &small);
    tag_readout = text(nfc, tr("spoolbuddy.dashboard.noTag"), 170, 12, 108, MUTED, &small);
    lv_obj_set_style_text_align(tag_readout, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_t *printers = rect(body, 14, 259, 316, 79, BG, 10);
    border(printers, 0x2b3137, 1);
    text(printers, tr("port.Printers"), 14, 14, 280, 0xa1a1aa, &small);
    if (sb_live.enabled) {
        lv_obj_add_flag(printers, LV_OBJ_FLAG_SCROLLABLE);
        const cJSON *p;
        int index = 0;
        cJSON_ArrayForEach(p, bridge_model("printers")) {
            lv_obj_t *pill = rect(printers, 14 + index++ * 142, 42, 134, 24, 0x202428, 5);
            char key[32];
            snprintf(key, sizeof(key), "printer_%d", (int)number(p, "id", 0));
            bool online = cJSON_IsTrue(prop(bridge_model(key), "connected"));
            circle(pill, 6, 9, 6, online ? GREEN : MUTED);
            text(pill, str(p, "name"), 17, 5, 112, TEXT, &small);
        }
        if (!index)
            text(printers, tr("common.noPrinters"), 14, 43, 280, MUTED,
                 &small);
    } else {
        lv_obj_t *pill = rect(printers, 14, 42, 48, 22, 0x202428, 5);
        circle(pill, 6, 8, 6, GREEN);
        text(pill, "P1S", 17, 5, 30, 0xa1a1aa, &small);
    }
    lv_obj_t *current = rect(body, 344, 63, 442, 270, BG, 10);
    border(current, 0x2b3137, 1);
    text(current, tr("port.CurrentSpool"), 22, 22, 396, 0xa1a1aa, &small);
    if (sb_live.enabled && sb_live.uid[0] && sb_live.spool_id <= 0) {
        center_text(current, tr("port.UnknownSpool"), 16, 78, 410, TEXT, &heading);
        center_text(current, sb_live.uid, 16, 118, 410, MUTED, &regular);
        button(current, tr("inventory.assignSpool"), 24, 176, 190, 44, GREEN, action_cb,
               ACT_LINK);
        button(current, tr("spoolbuddy.modal.addToInventory"), 228, 176, 190, 44, CARD, action_cb,
               ACT_UNKNOWN_ADD);
        return;
    }
    if (sb_live.enabled && sb_live.spool_id > 0) {
        char title[200];
        snprintf(title, sizeof(title), "%s %s", sb_live.brand, sb_live.material);
        const cJSON *s = spool_by_id(sb_live.spool_id);
        spool(current, 190, 48, 62, s ? spool_color(s) : GREEN);
        center_text(current, title, 16, 122, 410, TEXT, &heading);
        char amount[100];
        snprintf(amount, sizeof(amount), "%.0f g", (double)sb_live.remaining);
        center_text(current, amount, 16, 156, 410, MUTED, &regular);
        button(current, tr("port.Details"), 24, 203, 190, 42, SECONDARY, inventory_detail,
               sb_live.spool_id);
        button(current, tr("spoolbuddy.modal.syncWeight"), 226, 203, 190, 42, GREEN, action_cb,
               ACT_SYNC);
        original_live_changed();
        return;
    }
    if (sb_live.enabled && sb_live.uid[0]) {
        center_text(current, tr("port.UnknownTag"), 20, 70, 402, TEXT, &heading);
        center_text(current, sb_live.uid, 20, 112, 402, MUTED, &small);
        button(current, tr("port.LinkSpool_446b4d"), 20, 174, 195, 44, GREEN, action_cb, ACT_LINK);
        button(current, tr("spoolbuddy.modal.addToInventory"), 227, 174, 195, 44, SECONDARY,
               action_cb, ACT_CREATE);
        original_live_changed();
        return;
    }
    if (preview_online) {
        circle(current, 173, 48, 96, 0x192329);
        lv_obj_t *outer = circle(current, 179, 54, 84, GREEN);
        border(outer, 0xb0cbb5, 4);
        circle(current, 187, 62, 68, 0x1a9a49);
    } else {
        circle(current, 177, 52, 86, CARD);
        image(current, &sb_icon_offline, 200, 75);
    }
    center_text(current,
                preview_online ? tr("spoolbuddy.dashboard.readyToScan") : tr("spoolbuddy.status.deviceOffline"),
                12, 165, 418, preview_online ? TEXT : MUTED, &heading);
    center_text(current,
                preview_online
                    ? tr("spoolbuddy.dashboard.idleMessage")
                    : tr("spoolbuddy.status.connectDisplay"),
                12, 195, 418, MUTED, &small);
    const char *hint = preview_online
        ? tr("spoolbuddy.dashboard.nfcHint")
        : tr("spoolbuddy.status.waitingConnection");
    lv_point_t hint_size;
    lv_text_get_size(&hint_size, hint, &small, 0, 3, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    /* Center the icon and translated text together, rather than a fixed English offset. */
    int hint_x = (442 - (22 + hint_size.x)) / 2;
    image(current, &sb_icon_info, hint_x, 234);
    text(current, hint, hint_x + 22, 236, hint_size.x, 0x52525b, &small);
}
static void ams(void) {
    if (sb_live.enabled) {
        const cJSON *unit_data;
        int unit_index = 0;
        cJSON_ArrayForEach(unit_data, prop(bridge_model("printer_status"), "ams")) {
            int ams_id = (int)number(unit_data, "id", 0);
            char title[64];
            snprintf(title, sizeof(title), "AMS %c",
                     'A' + (ams_id >= 0 && ams_id < 26 ? ams_id : 0));
            lv_obj_t *unit = rect(body, 14 + (unit_index % 2) * 395, 14 + (unit_index / 2) * 175,
                                  381, 159, SECONDARY, 10);
            border(unit, BORDER, 1);
            text(unit, title, 12, 14, 210, TEXT, &regular);
            snprintf(title, sizeof(title), "%.1f °C  %.0f%%", number(unit_data, "temp", 0),
                     number(unit_data, "humidity", 0));
            text(unit, title, 260, 16, 110, MUTED, &small);
            const cJSON *tray;
            int index = 0;
            cJSON_ArrayForEach(tray, prop(unit_data, "tray")) {
                if (index >= 4)
                    break;
                int tray_id = number(tray, "id", index);
                lv_obj_t *slot = button(unit, "", 15 + index * 91, 46, 76, 96, SECONDARY, action_cb,
                                        200000 + ams_id * 256 + tray_id);
                const char *hex = str(tray, "tray_color");
                unsigned color =
                    strlen(hex) == 8 ? (unsigned)strtoul(hex, NULL, 16) >> 8 : 0x808080;
                spool(slot, 8, 5, 56, color);
                char value[12];
                snprintf(value, sizeof(value), "%d", tray_id + 1);
                lv_obj_t *badge = rect(slot, 58, 0, 16, 19, BORDER, 3);
                center_text(badge, value, 0, 3, 16, MUTED, &small);
                center_text(slot,
                            str(tray, "tray_type")[0] ? str(tray, "tray_type") : tr("spoolbuddy.ams.empty"),
                            0, 70, 76, MUTED, &small);
                index++;
            }
            unit_index++;
        }
        if (!unit_index)
            center_text(body, tr("port.NoAmsDataAvailable"), 14, 90, 772, MUTED,
                        &regular);
        const cJSON *ext;
        int ext_index = 0;
        cJSON_ArrayForEach(ext, prop(bridge_model("printer_status"), "vt_tray")) {
            lv_obj_t *row =
                button(body, "", 14 + (ext_index % 2) * 198, 14 + ((unit_index + 1) / 2) * 175, 185,
                       50, SECONDARY, action_cb, 200000 + 254 * 256 + (int)number(ext, "id", 254));
            text(row, tr("printers.ext"), 57, 8, 116, MUTED, &small);
            text(row, str(ext, "tray_type")[0] ? str(ext, "tray_type") : tr("spoolbuddy.ams.empty"), 57, 25,
                 116, TEXT, &small);
            ext_index++;
        }
        lv_obj_add_flag(body, LV_OBJ_FLAG_SCROLLABLE);
        return;
    }
    lv_obj_t *unit = rect(body, 14, 14, 381, 159, SECONDARY, 10);
    border(unit, BORDER, 1);
    text(unit, "AMS A", 12, 14, 230, 0xf4f4f5, &regular);
    text(unit, "— °C   — %", 270, 16, 100, MUTED, &small);
    unsigned colors[] = {0xbc0900, 0x000000, 0xffffff, 0xfff144};
    const char *materials[] = {"PETG", "PETG", "PLA", "PLA"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *slot = button(unit, "", 15 + i * 91, 46, 76, 96, SECONDARY, noop, i);
        spool(slot, 8, 5, 56, colors[i]);
        lv_obj_t *badge = rect(slot, 58, 0, 16, 19, 0x363e46, 3);
        char n[2] = {(char)('1' + i), 0};
        center_text(badge, n, 0, 3, 16, 0xa1a1aa, &small);
        center_text(slot, materials[i], 0, 70, 76, 0x9ca3af, &small);
    }
    lv_obj_t *ext = rect(body, 14, 185, 185, 50, SECONDARY, 6);
    lv_obj_t *ring = circle(ext, 12, 9, 32, SECONDARY);
    border(ring, 0x71717a, 2);
    circle(ring, 12, 12, 5, 0x52525b);
    text(ext, tr("printers.ext"), 57, 8, 116, 0x9ca3af, &small);
    text(ext, tr("spoolbuddy.ams.empty"), 57, 25, 116, TEXT, &small);
}
static void form_row(lv_obj_t *p, const char *caption, const char *placeholder, int y) {
    text(p, caption, 14, y, 360, 0xa1a1aa, &small);
    if (!strcmp(caption, tr("inventory.storageLocation")) && sb_live.enabled) {
        location_select = lv_dropdown_create(p);
        lv_obj_set_pos(location_select, 14, y + 19);
        lv_obj_set_size(location_select, 390, 34);
        style(location_select, BG, 3);
        lv_dropdown_set_symbol(location_select, &sb_icon_chevron);
        lv_obj_set_user_data(location_select, (void *)"location_id");
        location_choices();
        lv_obj_add_event_cb(location_select, location_changed, LV_EVENT_VALUE_CHANGED, NULL);
        return;
    }
    lv_obj_t *input = field(p, placeholder, 14, y + 19, 390, 34);
    if (!strcmp(caption, tr("inventory.coreWeight")) && sb_live.enabled) {
        lv_obj_set_width(input, 274);
        button(p, tr("port.Catalog"), 300, y + 19, 104, 34, BG, core_catalog_open, 0);
    }
    const char *en[] = {tr("port.SlicerPreset"), tr("port.Material"), tr("port.Brand"), tr("port.Subtype"), tr("inventory.labelWeight"), tr("spoolbuddy.writeTag.colorName"), tr("inventory.hexColor"), tr("inventory.extraColorsLabel"), tr("inventory.coreWeight"), tr("inventory.currentWeight"), tr("inventory.measuredWeight"), tr("inventory.costPerKg"), tr("inventory.materialNumber"), tr("inventory.category"), tr("inventory.lowStockThresholdOverride"), tr("inventory.storageLocation"), tr("spoolbuddy.inventory.note"), tr("spoolbuddy.spool.brand"), tr("spoolbuddy.writeTag.weight")};
    const char *keys[] = {"slicer_filament_name",
                          "material",
                          "brand",
                          "subtype",
                          "label_weight",
                          "color_name",
                          "rgba",
                          "extra_colors",
                          "core_weight",
                          "remaining_weight",
                          "measured_weight",
                          "cost_per_kg",
                          "material_number",
                          "category",
                          "low_stock_threshold_pct",
                          "storage_location",
                          "note",
                          "brand",
                          "label_weight"};
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
        if (!strcmp(caption, en[i])) {
            draft_bind(input, keys[i]);
            break;
        }
}
static void write_select(lv_event_t *e) {
    selected_spool = (int)(intptr_t)lv_event_get_user_data(e);
    draw_page();
}
static void write_preview(void) {
    if (!write_panel)
        return;
    lv_obj_clean(write_panel);
    if (bridge_connected()) {
        const cJSON *selected = spool_by_id(selected_spool);
        if (selected && write_tab != 1) {
            spool(write_panel, 136, 31, 68, spool_color(selected));
            char caption[180];
            snprintf(caption, sizeof(caption), "%s %s", str(selected, "brand"),
                     str(selected, "material"));
            center_text(write_panel, caption, 12, 113, 316, TEXT, &regular);
            center_text(write_panel, str(selected, "color_name"), 12, 144, 316, MUTED, &small);
            button(write_panel, tr("port.WriteTag"), 20, 192, 300, 43, GREEN, action_cb,
                   ACT_WRITE);
            if (write_tab == 2 && (str(selected, "tag_uid")[0] || str(selected, "tray_uuid")[0]))
                button(write_panel, tr("spoolbuddy.writeTag.untagSpool"), 20, 244, 300,
                       40, CARD, action_cb, ACT_UNLINK);
        } else
            center_text(write_panel,
                        write_tab == 1 ? tr("port.CreateASpoolUsingTheForm")
                                       : tr("port.SelectASpoolToWrite"),
                        12, 121, 316, MUTED, &regular);
        if (!strcmp(str(own_device(), "pending_command"), "write_tag")) {
            center_text(
                write_panel,
                tr("port.PlaceAWritableNfcTagOnTheReader"), 12,
                162, 316, TEXT, &small);
            button(write_panel, tr("port.CancelWrite"), 20, 244, 300, 40, CARD, action_cb,
                   ACT_CANCEL_WRITE);
        }
    } else {
        image(write_panel, &sb_icon_wifi, 150, 122);
        center_text(write_panel, tr("spoolbuddy.writeTag.deviceOffline"), 12, 173, 316,
                    MUTED, &small);
    }
}
static void write_rows(void) {
    if (!write_list)
        return;
    lv_obj_clean(write_list);
    int index = 0;
    bool selected_visible = false;
    const cJSON *s;
    cJSON_ArrayForEach(s, bridge_model("inventory")) {
        if (str(s, "archived_at")[0])
            continue;
        if (write_tab == 0 && str(s, "tag_uid")[0])
            continue;
        if (write_tab == 2 && !str(s, "tag_uid")[0] && !str(s, "tray_uuid")[0])
            continue;
        if (!contains(str(s, "brand"), write_query) && !contains(str(s, "material"), write_query) &&
            !contains(str(s, "subtype"), write_query) &&
            !contains(str(s, "color_name"), write_query))
            continue;
        int id = number(s, "id", 0);
        if (id == selected_spool)
            selected_visible = true;
        lv_obj_t *row = button(write_list, "", 0, index++ * 80, 437, 72,
                               id == selected_spool ? 0x153c2e : SECONDARY, write_select, id);
        circle(row, 12, 22, 28, spool_color(s));
        char caption[180];
        snprintf(caption, sizeof(caption), "%s %s %s", str(s, "brand"), str(s, "material"),
                 str(s, "subtype"));
        text(row, caption, 51, 13, 374, TEXT, &small);
        text(row, str(s, "color_name"), 51, 34, 374, MUTED, &small);
    }
    if (!selected_visible)
        selected_spool = 0;
    write_preview();
    if (!index)
        center_text(write_list, tr("spoolbuddy.inventory.noResults"), 0, 31, 433, MUTED,
                    &small);
}
static void write_search_changed(lv_event_t *e) {
    snprintf(write_query, sizeof(write_query), "%s",
             lv_textarea_get_text(lv_event_get_target_obj(e)));
    write_rows();
}
static void write_page(void) {
    pa_list = NULL;
    if (full_form && sb_live.enabled)
        bridge_load_catalogs();
    const char *en[] = {tr("spoolbuddy.writeTag.tabExisting"), tr("spoolbuddy.writeTag.tabNew"), tr("spoolbuddy.writeTag.replaceTag")};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = button(body, en[i], i * 267, 0, i == 2 ? 266 : 267, 42, BG,
                             write_cb, i);
        lv_obj_set_style_text_color(b, lv_color_hex(write_tab == i ? GREEN : 0xa1a1aa), 0);
        if (write_tab == i)
            rect(body, i * 267, 40, 267, 2, GREEN, 0);
    }
    rect(body, 0, 42, 800, 1, BORDER, 0);
    rect(body, 459, 42, 1, 306, BORDER, 0);
    lv_obj_t *left = rect(body, 0, 43, 459, 305, BG, 0);
    lv_obj_add_flag(left, LV_OBJ_FLAG_SCROLLABLE);
    if (write_tab == 1) {
        if (sb_live.enabled && !draft)
            draft_reset();
        lv_obj_t *view = rect(left, 11, 10, 422, 44, SECONDARY, 5);
        border(view, BORDER, 1);
        text(view, tr("spoolbuddy.writeTag.viewMode"), 8, 14, 245, TEXT, &small);
        button(view, tr("spoolbuddy.writeTag.simpleView"), 316, 9, 53, 26, full_form ? BG : 0x194b34, view_cb, 0);
        button(view, tr("spoolbuddy.writeTag.fullView"), 369, 9, 44, 26, full_form ? 0x194b34 : BG, view_cb, 1);
        if (full_form) {
            lv_obj_t *stock = rect(left, 11, 64, 422, 38, SECONDARY, 5);
            border(stock, BORDER, 1);
            text(stock, tr("inventory.quickAdd"), 8, 11, 320, TEXT, &small);
            lv_obj_t *toggle = lv_switch_create(stock);
            lv_obj_set_pos(toggle, 375, 9);
            lv_obj_set_size(toggle, 37, 20);
            lv_obj_set_user_data(toggle, (void *)"quick_add");
            if (quick_add)
                lv_obj_add_state(toggle, LV_STATE_CHECKED);
            lv_obj_add_event_cb(toggle, quick_add_changed, LV_EVENT_VALUE_CHANGED, NULL);
            lv_obj_t *tabs = rect(left, 11, 113, 422, 38, SECONDARY, 5);
            border(tabs, BORDER, 1);
            button(tabs, tr("inventory.filamentInfoTab"), 1, 1, 209, 36,
                   pa_profile ? SECONDARY : 0x153c2e, profile_cb, 0);
            if (!quick_add)
                button(tabs, tr("inventory.paProfileTab"), 211, 1, 209, 36,
                       pa_profile ? 0x153c2e : SECONDARY, profile_cb, 1);
            lv_obj_t *full = rect(left, 11, 163, 422,
                                  pa_profile  ? 540
                                  : quick_add ? 1605
                                              : 1535,
                                  SECONDARY, 5);
            border(full, BORDER, 1);
            if (pa_profile) {
                text(full, tr("inventory.paProfileTab"), 12, 14, 230, TEXT, &regular);
                button(full, tr("inventory.autoSelect"), 267, 8, 140, 32, BG, pa_auto, 0);
                pa_list = rect(full, 8, 52, 406, 476, SECONDARY, 0);
                lv_obj_add_flag(pa_list, LV_OBJ_FLAG_SCROLLABLE);
                pa_render();
            } else {
                bool cloud = cJSON_IsTrue(prop(bridge_model("cloud_auth"), "is_authenticated")) ||
                             cJSON_IsTrue(prop(bridge_model("orca_auth"), "connected"));
                text(full,
                     cloud ? tr("inventory.cloudConnected")
                           : tr("inventory.cloudNotConnected"),
                     12, 12, 390, MUTED, &small);
                const char *en[] = {tr("port.SlicerPreset"), tr("port.Material"), tr("port.Brand"), tr("port.Subtype"), tr("inventory.labelWeight")};
                for (int i = 0; i < 5; i++) {
                    if (i == 0 && sb_live.enabled) {
                        text(full, tr("port.SlicerPreset"), 14, 44, 390, MUTED, &small);
                        button(full,
                               str(draft, "slicer_filament_name")[0]
                                   ? str(draft, "slicer_filament_name")
                                   : tr("inventory.searchPresets"),
                               14, 63, 390, 34, BG, preset_open, 0);
                        continue;
                    }
                    form_row(full, en[i],
                             i == 0   ? tr("inventory.searchPresets")
                             : i == 4 ? "—"
                                      : "",
                             44 + i * 70);
                }
                lv_obj_t *color_search =
                    field(full, tr("inventory.searchColors"), 14, 410, 390, 34);
                lv_obj_set_user_data(color_search, (void *)"quick_color_search");
                lv_obj_add_event_cb(color_search, quick_color_search, LV_EVENT_VALUE_CHANGED, NULL);
                text(full, tr("inventory.commonColors"), 14, 459, 270, MUTED, &small);
                button(full, tr("archives.showAll"), 315, 450, 89, 27, SECONDARY, action_cb,
                       700);
                for (int i = 0; i < 13; i++) {
                    lv_obj_t *swatch =
                        circle(full, 14 + (i % 10) * 39, 487 + (i / 10) * 37, 27, quick_colors[i]);
                    quick_swatches[i] = swatch;
                    border(swatch, BORDER, 1);
                    lv_obj_add_flag(swatch, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_add_event_cb(
                        swatch, color_choose, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)(i == 12 ? UINTPTR_MAX : quick_colors[i]));
                }
                form_row(full, tr("spoolbuddy.writeTag.colorName"), "", 572);
                form_row(full, tr("inventory.hexColor"), "#808080", 642);
                form_row(full, tr("inventory.extraColorsLabel"), "", 712);
                text(full,
                     tr("port.Paste2To8HexStopsSeparatedByCommas"),
                     14, 766, 390, MUTED, &small);
                text(full, tr("inventory.colorEffectLabel"), 14, 800, 390, MUTED, &small);
                lv_obj_t *effects = lv_dropdown_create(full);
                lv_obj_set_pos(effects, 14, 820);
                lv_obj_set_size(effects, 390, 34);
                style(effects, BG, 3);
                lv_dropdown_set_symbol(effects, &sb_icon_chevron);
                lv_dropdown_set_options(
                    effects, "None\nSparkle\nWood\nMarble\nGlow\nMatte\nSilk\nGalaxy\nRainbow\nMeta"
                             "l\nTranslucent\nGradient\nDual Color\nTri Color\nMulticolor");
                lv_obj_add_event_cb(effects, dropdown_draft, LV_EVENT_VALUE_CHANGED,
                                    (void *)"effect_type");
                const char *bottom_en[] = {tr("inventory.coreWeight"), tr("inventory.currentWeight"), tr("inventory.measuredWeight"), tr("inventory.costPerKg"), tr("inventory.materialNumber"), tr("inventory.category"), tr("inventory.lowStockThresholdOverride"), tr("inventory.storageLocation"), tr("spoolbuddy.inventory.note")};
                for (int i = 0; i < 9; i++)
                    form_row(full, bottom_en[i], i < 3 ? "—" : "",
                             880 + i * 70);
            }
            if (quick_add) {
                text(full, tr("queue.quantity"), 14, 1535, 390, MUTED, &small);
                draft_bind(field(full, "1", 14, 1554, 390, 34), "quantity");
            }
            button(left, tr("spoolbuddy.writeTag.createSpool"), 11,
                   pa_profile  ? 720
                   : quick_add ? 1780
                               : 1710,
                   422, 39, 0x18472d, action_cb, ACT_CREATE);
        } else {
            lv_obj_t *form = rect(left, 11, 64, 422, 342, SECONDARY, 5);
            border(form, BORDER, 1);
            text(form, tr("spoolbuddy.spool.material"), 14, 14, 380, 0xa1a1aa, &small);
            lv_obj_t *drop = lv_dropdown_create(form);
            lv_obj_set_pos(drop, 14, 34);
            lv_obj_set_size(drop, 390, 34);
            style(drop, 0x363e46, 3);
            lv_dropdown_set_options(drop, "PLA\nPETG\nABS\nASA\nTPU\nPA\nPC\nPVA\nHIPS");
            lv_obj_add_event_cb(drop, dropdown_draft, LV_EVENT_VALUE_CHANGED, (void *)"material");
            if (sb_live.enabled && draft) {
                int selected = lv_dropdown_get_option_index(drop, str(draft, "material"));
                if (selected >= 0)
                    lv_dropdown_set_selected(drop, selected);
            }
            lv_dropdown_set_symbol(drop, &sb_icon_chevron);
            lv_obj_set_style_pad_left(drop, 14, 0);
            lv_obj_set_style_pad_top(drop, 7, 0);
            text(form, tr("spoolbuddy.writeTag.colorName"), 14, 82, 320, 0xa1a1aa, &small);
            text(form, tr("spoolbuddy.spool.color"), 353, 82, 52, 0xa1a1aa, &small);
            draft_bind(field(form, "Jade White", 14, 101, 344, 35), "color_name");
            lv_obj_t *swatch =
                button(form, "", 370, 104, 30, 26,
                       sb_live.enabled && draft ? spool_color(draft) : 0x808080, action_cb, 700);
            border(swatch, BORDER, 1);
            form_row(form, tr("spoolbuddy.spool.brand"), "Polymaker", 150);
            form_row(form, tr("spoolbuddy.writeTag.weight"), "—", 219);
            lv_obj_t *create = button(left, tr("spoolbuddy.writeTag.createSpool"), 11, 418, 422, 39,
                                      0x18472d, action_cb, ACT_CREATE);
            lv_obj_set_style_text_color(create, lv_color_hex(MUTED), 0);
        }
    } else {
        lv_obj_t *search =
            field(left, tr("spoolbuddy.writeTag.searchPlaceholder"), 11,
                  11, 437, 34);
        if (sb_live.enabled) {
            lv_textarea_set_text(search, write_query);
            lv_obj_set_user_data(search, (void *)"write_search");
            lv_obj_add_event_cb(search, write_search_changed, LV_EVENT_VALUE_CHANGED, NULL);
            write_list = rect(left, 11, 56, 437, 249, BG, 0);
            lv_obj_add_flag(write_list, LV_OBJ_FLAG_SCROLLABLE);
            write_rows();
        } else if (write_tab == 0)
            center_text(left, tr("spoolbuddy.writeTag.noUntaggedSpools"), 12, 87, 433,
                        MUTED, &small);
        else
            for (int i = 0; i < 2; i++) {
                lv_obj_t *row = button(left, "", 11, 56 + i * 80, 437, 72, SECONDARY, noop, 0);
                circle(row, 12, 22, 28, i ? 0xffffff : 0xbc0900);
                text(row, i ? "Bambu Lab PLA Basic" : "Bambu Lab PETG HF", 51, 13, 374, TEXT,
                     &small);
                text(row, i ? "Jade White   —" : "—", 51, 32, 374, MUTED, &small);
                text(row, "—", 51, 49, 374, MUTED, &small);
            }
    }
    if (sb_live.enabled) {
        write_panel = rect(body, 460, 43, 340, 305, BG, 0);
        write_preview();
    } else {
        image(body, &sb_icon_wifi, 610, 165);
        center_text(body, tr("spoolbuddy.writeTag.deviceOffline"), 472, 216, 316, MUTED,
                    &small);
    }
}
static void inventory_detail(lv_event_t *e) {
    if (sb_live.enabled) {
        detail_show((int)(intptr_t)lv_event_get_user_data(e));
        return;
    }
    if (overlay)
        return;
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    overlay = rect(lv_screen_active(), 0, 0, 800, 480, 0x000000, 0);
    lv_obj_set_style_bg_opa(overlay, 180, 0);
    lv_obj_add_event_cb(overlay, close_overlay, LV_EVENT_CLICKED, NULL);
    lv_obj_t *card = rect(overlay, 204, 52, 392, 366, SECONDARY, 14);
    border(card, BORDER, 1);
    button(card, "×", 346, 10, 32, 30, SECONDARY, close_overlay, 0);
    spool(card, 161, 32, 70, i ? 0xffffff : 0xbc0900);
    center_text(card, i ? "PLA Basic" : "PETG HF", 20, 118, 352, TEXT, &heading);
    center_text(card, "Bambu Lab", 20, 150, 352, MUTED, &regular);
    center_text(card, "—", 20, 192, 352, MUTED, &heading);
    button(card, tr("spoolbuddy.modal.assignToAms"), 20, 250, 352, 40, 0x17432c, noop, 0);
}
static bool contains(const char *haystack, const char *needle) {
    if (!needle[0])
        return true;
    for (; *haystack; haystack++) {
        size_t i = 0;
        while (needle[i] && haystack[i] &&
               tolower((unsigned char)haystack[i]) == tolower((unsigned char)needle[i]))
            i++;
        if (!needle[i])
            return true;
    }
    return false;
}
static const cJSON *assignment(int spool_id) {
    const cJSON *a;
    cJSON_ArrayForEach(a, bridge_model("assignments")) {
        if ((int)number(a, "spool_id", number(a, "spoolman_spool_id", 0)) == spool_id)
            return a;
    }
    return NULL;
}
static void inventory_cards(void) {
    if (!inventory_grid)
        return;
    lv_obj_clean(inventory_grid);
    int index = 0;
    const cJSON *s;
    cJSON_ArrayForEach(s, bridge_model("inventory")) {
        int id = number(s, "id", 0);
        const cJSON *a = assignment(id);
        const char *material = str(s, "material");
        if ((filter == 1 && !a) || (material_filter[0] && strcmp(material, material_filter)))
            continue;
        if (!contains(material, search_query) && !contains(str(s, "brand"), search_query) &&
            !contains(str(s, "color_name"), search_query) &&
            !contains(str(s, "subtype"), search_query))
            continue;
        lv_obj_t *card = button(inventory_grid, "", 11 + (index % 5) * 157, (index / 5) * 180, 150,
                                172, SECONDARY, inventory_detail, id);
        lv_obj_set_style_radius(card, 12, 0);
        spool(card, 47, 12, 56, spool_color(s));
        char title[160];
        snprintf(title, sizeof(title), "%s %s", material, str(s, "subtype"));
        center_text(card, title, 8, 74, 134, TEXT, &small);
        center_text(card, str(s, "color_name")[0] ? str(s, "color_name") : "-", 8, 96, 134, MUTED,
                    &small);
        double label = number(s, "label_weight", 0),
               remaining = fmax(0, label - number(s, "weight_used", 0));
        double fraction = label > 0 ? fmin(1, remaining / label) : 0;
        rect(card, 12, 114, 126, 5, BORDER, 3);
        rect(card, 12, 114, (int)(126 * fraction), 5,
             fraction > .5   ? GREEN
             : fraction > .2 ? 0xeab308
                             : 0xef4444,
             3);
        snprintf(title, sizeof(title), "%.0fg (%.0f%%)", remaining, fraction * 100);
        center_text(card, title, 12, 124, 126, MUTED, &small);
        if (a) {
            int ams = number(a, "ams_id", 0), tray = number(a, "tray_id", 0);
            snprintf(title, sizeof(title), "%c%d", 'A' + ams, tray + 1);
            lv_obj_t *badge = rect(card, 57, 145, 36, 18, 0x18432d, 3);
            center_text(badge, title, 0, 2, 36, GREEN, &small);
        }
        index++;
    }
    if (!index)
        center_text(inventory_grid, tr("spoolbuddy.inventory.noResults"), 14, 60, 772, MUTED,
                    &regular);
}
static void search_changed(lv_event_t *e) {
    snprintf(search_query, sizeof(search_query), "%s",
             lv_textarea_get_text(lv_event_get_target_obj(e)));
    inventory_cards();
}
static void inventory(void) {
    lv_obj_t *search = field(body, tr("spoolbuddy.inventory.search"), 11, 11, 778, 34);
    lv_obj_set_style_bg_color(search, lv_color_hex(SECONDARY), 0);
    border(search, BORDER, 1);
    lv_obj_set_style_pad_left(search, 32, 0);
    image(body, &sb_icon_search, 21, 20);
    char all_count[48], ams_count[48];
    const char *captions[34] = {tr("port.All2"), tr("port.InAms2"), "PETG",
                                "PLA"};
    int count = 4;
    material_count = 0;
    if (sb_live.enabled) {
        int in_ams = 0;
        const cJSON *item;
        cJSON_ArrayForEach(item, bridge_model("inventory")) {
            if (assignment((int)number(item, "id", 0)))
                in_ams++;
            const char *name = str(item, "material");
            bool exists = false;
            for (int i = 0; i < material_count; i++)
                if (!strcmp(material_options[i], name))
                    exists = true;
            if (name[0] && !exists && material_count < 32)
                snprintf(material_options[material_count++], 64, "%s", name);
        }
        snprintf(all_count, sizeof(all_count), "%s (%d)", tr("spoolbuddy.inventory.all"),
                 cJSON_GetArraySize(bridge_model("inventory")));
        snprintf(ams_count, sizeof(ams_count), "%s (%d)", tr("spoolbuddy.inventory.inAms"), in_ams);
        captions[0] = all_count;
        captions[1] = ams_count;
        count = material_count + 2;
        filter = filter == 1 ? 1 : 0;
        for (int i = 0; i < material_count; i++) {
            captions[i + 2] = material_options[i];
            if (!strcmp(material_options[i], material_filter))
                filter = i + 2;
        }
        if (material_filter[0] && filter < 2)
            material_filter[0] = 0;
    }
    lv_obj_t *filters = rect(body, 11, 55, 778, 33, BG, 0);
    lv_obj_add_flag(filters, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(filters, LV_DIR_HOR);
    int x = 0;
    for (int i = 0; i < count; i++) {
        int width = i == 0 ? 85 : i == 1 ? 105 : (int)strlen(captions[i]) * 9 + 26;
        lv_obj_t *b = button(filters, captions[i], x, 0, width, 31, filter == i ? 0x123d2c : BG,
                             filter_cb, i);
        lv_obj_set_style_radius(b, 16, 0);
        border(b, filter == i ? 0x1b5937 : BORDER, 1);
        lv_obj_set_style_text_color(b, lv_color_hex(filter == i ? GREEN : 0x9ca3af), 0);
        x += width + 7;
    }
    if (sb_live.enabled) {
        lv_textarea_set_text(search, search_query);
        lv_obj_set_user_data(search, (void *)"inventory_search");
        lv_obj_add_event_cb(search, search_changed, LV_EVENT_VALUE_CHANGED, NULL);
        inventory_grid = rect(body, 0, 94, 800, 254, BG, 0);
        lv_obj_add_flag(inventory_grid, LV_OBJ_FLAG_SCROLLABLE);
        inventory_cards();
        return;
    }
    for (int i = 0, col = 0; i < 2; i++) {
        if ((filter == 2 && i == 1) || (filter == 3 && i == 0))
            continue;
        lv_obj_t *card =
            button(body, "", 11 + (col++) * 157, 94, 150, 172, SECONDARY, inventory_detail, i);
        lv_obj_set_style_radius(card, 12, 0);
        spool(card, 47, 12, 56, i ? 0xffffff : 0xbc0900);
        center_text(card, i ? "PLA Basic" : "PETG HF", 8, 74, 134, TEXT, &small);
        circle(card, i ? 38 : 66, 98, 8, i ? 0xffffff : 0xbc0900);
        center_text(card, i ? "Jade White" : "-", i ? 49 : 77, 96, i ? 79 : 12, MUTED, &small);
        rect(card, 12, 114, 126, 5, BORDER, 3);
        center_text(card, "—", 12, 124, 126, MUTED, &small);
        lv_obj_t *tag = rect(card, 62, 145, 26, 18, 0x18432d, 3);
        center_text(tag, i ? "A3" : "A1", 0, 2, 26, GREEN, &small);
    }
}
#include "ui_system.h"
#include "ui_updates.h"
static void settings(void) {
    text(body, tr("spoolbuddy.nav.settings"), 14, 16, 772, 0xf4f4f5, &heading);
    lv_obj_t *tabs = rect(body, 14, 51, 772, 43, 0x202428, 6);
    const char *en[] = {tr("spoolbuddy.dashboard.device"), tr("spoolbuddy.settings.tabDisplay"), tr("spoolbuddy.settings.scale"), tr("spoolbuddy.settings.tabUpdates"), tr("spoolbuddy.settings.tabSystem")};
    for (int i = 0; i < 5; i++) {
        lv_obj_t *b = button(tabs, en[i], 4 + i * 153, 4, 150, 35,
                             settings_tab == i ? 0x3f3f46 : 0x202428, settings_cb, i);
        lv_obj_set_style_text_color(b, lv_color_hex(settings_tab == i ? TEXT : MUTED), 0);
    }
    lv_obj_t *content = rect(body, 14, 109, 772, 226, BG, 0);
    settings_content=content;
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    if (settings_tab == 0) {
        lv_obj_t *nfc = rect(content, 0, 0, 375, 120, CARD, 6);
        text(nfc, tr("spoolbuddy.settings.nfcReader"), 11, 12, 353, TEXT, &regular);
        const char *enlabels[] = {tr("spoolbuddy.settings.type"), tr("spoolbuddy.settings.connection"), tr("spoolbuddy.status.status")};
        for (int i = 0; i < 3; i++) {
            text(nfc, enlabels[i], 11, 38 + i * 21, 190, MUTED, &small);
            const cJSON *d = own_device();
            const char *values[] = {str(d, "nfc_reader_type"), str(d, "nfc_connection"),
                                    cJSON_IsTrue(prop(d, "nfc_ok")) ? tr("spoolbuddy.status.nfcReady")
                                                                    : tr("port.NotReady")};
            lv_obj_t *v =
                text(nfc, sb_live.enabled ? values[i] : "—", 155, 38 + i * 21, 209, MUTED, &small);
            lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
        }
        lv_obj_t *device = rect(content, 382, 0, 377, 120, CARD, 6);
        text(device, tr("spoolbuddy.settings.deviceInfo"), 11, 12, 355, TEXT, &regular);
        const char *dn[] = {tr("spoolbuddy.settings.hostname"), "IP", tr("spoolbuddy.settings.uptime"), "ID"};
        for (int i = 0; i < 4; i++) {
            text(device, dn[i], 11, 38 + i * 21, 180, MUTED, &small);
            const cJSON *d = own_device();
            char uptime[40];
            snprintf(uptime, sizeof(uptime), "%.0f s", number(d, "uptime_s", 0));
            const char *values[] = {str(d, "hostname"), str(d, "ip_address"), uptime,
                                    str(d, "device_id")};
            lv_obj_t *v = text(device, sb_live.enabled ? values[i] : "—", 142, 38 + i * 21, 222,
                               MUTED, &small);
            lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
        }
        lv_obj_t *auth = rect(content, 0, 127, 375, 126, CARD, 6);
        text(auth, tr("spoolbuddy.settings.systemConfig"), 11, 12, 353, TEXT, &regular);
        lv_obj_t *url = field(auth, "http://192.168.1.100:5000", 11, 37, 353, 27);
        lv_obj_t *token = field(auth, tr("spoolbuddy.settings.apiTokenPlaceholder"), 11, 72, 261, 27);
        if (sb_live.enabled) {
            lv_textarea_set_text(url, sb_settings.backend_url);
            lv_textarea_set_text(token, tr("port.ConfiguredOnDevice"));
            lv_obj_remove_flag(url, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_remove_flag(token, LV_OBJ_FLAG_CLICKABLE);
        }
        button(auth, tr("spoolbuddy.settings.saveConfig"), 280, 72, 84, 27, 0x185a33, action_cb,
               ACT_SAVE_CONFIG);
        const char *di[] = {tr("spoolbuddy.settings.nfcDiagnostic"), tr("spoolbuddy.diagnostic.scaleTitle"), tr("spoolbuddy.diagnostic.readTagTitle")};
        unsigned colors[] = {0x1746cc, 0x956000, 0x007554};
        for (int i = 0; i < 3; i++)
            button(content, di[i], 393, 137 + i * 37, 353, 30, colors[i],
                   action_cb, ACT_DIAG_NFC + i);
    } else if (settings_tab == 1) {
        lv_obj_t *b = rect(content, 0, 0, 759, 75, CARD, 6);
        text(b, tr("spoolbuddy.settings.brightness"), 14, 14, 700, TEXT, &regular);
        lv_obj_t *slider = lv_slider_create(b);
        lv_obj_set_pos(slider, 40, 48);
        lv_obj_set_size(slider, 648, 6);
        lv_slider_set_value(slider, 100, LV_ANIM_OFF);
        if (sb_live.enabled) {
            lv_slider_set_value(slider, sb_live.brightness, LV_ANIM_OFF);
            lv_obj_add_event_cb(slider, brightness_changed, LV_EVENT_RELEASED, NULL);
        }
        lv_obj_set_style_bg_color(slider, lv_color_hex(GREEN), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider, lv_color_hex(GREEN), LV_PART_KNOB);
        char percent[16];
        snprintf(percent, sizeof(percent), "%d%%", sb_live.enabled ? sb_live.brightness : 100);
        text(b, percent, 714, 43, 40, TEXT, &small);
        lv_obj_t *timeout = rect(content, 0, 89, 759, 168, CARD, 6);
        text(timeout, tr("spoolbuddy.settings.screenBlank"), 14, 14, 730, TEXT, &regular);
        text(
            timeout,
            tr("spoolbuddy.settings.screenBlankDesc"),
            14, 39, 730, MUTED, &small);
        const char *options[] = {"Off", "1m", "2m", "5m", "10m", "30m"};
        if (sb_live.enabled) {
            const int seconds[] = {0, 60, 120, 300, 600, 1800};
            for (int i = 0; i < 6; i++)
                if (sb_live.blank_seconds == seconds[i])
                    blank_option = i;
        }
        for (int i = 0; i < 6; i++)
            button(timeout, i == 0 ? tr("common.off") : options[i], 14 + (i % 3) * 245,
                   62 + (i / 3) * 47, 237, 39, blank_option == i ? GREEN : 0x3f3f46, blank_cb, i);
    } else if (settings_tab == 2) {
        lv_obj_t *row = rect(content, 0, 0, 759, 100, CARD, 6);
        text(row,tr("spoolbuddy.settings.currentWeight"),14,15,350,MUTED,&regular);
        settings_stability=circle(row,580,20,8,sb_live.stable?GREEN:0xf59e0b);
        char value[64]="— g";
        if(valid_weight())snprintf(value,sizeof(value),"%.1f g",(double)gross());
        settings_weight=text(row,value,606,10,138,TEXT,&heading);
        lv_obj_set_style_text_align(settings_weight,LV_TEXT_ALIGN_RIGHT,0);
        const cJSON *device=own_device();
        text(row,tr("spoolbuddy.settings.tareOffset"),14,62,130,MUTED,&small);
        snprintf(value,sizeof(value),"%.0f",number(device,"tare_offset",0));
        text(row,value,155,62,190,TEXT,&small);
        text(row,tr("spoolbuddy.settings.calFactor"),390,62,170,MUTED,&small);
        snprintf(value,sizeof(value),"%.2f",number(device,"calibration_factor",1));
        lv_obj_t *factor=text(row,value,565,62,179,TEXT,&small);
        lv_obj_set_style_text_align(factor,LV_TEXT_ALIGN_RIGHT,0);
        button(content, tr("spoolbuddy.weight.tare"), 0, 111, 375, 44, 0x3f3f46, action_cb, ACT_TARE);
        button(content, tr("spoolbuddy.weight.calibrate"), 383, 111, 376, 44, GREEN, action_cb, ACT_CALIBRATE);
    } else if (settings_tab == 3) {
        update_settings(content);
    } else {
        system_settings(content);
    }
}
static void update_clock(lv_timer_t *timer) {
    (void)timer;
    if (sb_live.enabled)
        ui_idle_tick();
    time_t now = time(NULL);
    char value[20] = "—:—";
    if (now > 1700000000) {
        struct tm *t = localtime(&now);
        if (t) {
            if (!strcmp(str(bridge_model("app_settings"),"time_format"),"12h"))
                snprintf(value,sizeof(value),"%d:%02d %s",t->tm_hour%12 ? t->tm_hour%12 : 12,
                         t->tm_min,t->tm_hour<12 ? "AM" : "PM");
            else strftime(value,sizeof(value),"%H:%M",t);
        }
    }
    lv_label_set_text(clock_label, value);
    static uint32_t update_revision;
    sb_update_state update;sb_update_snapshot(&update);
    if(update.revision!=update_revision) {
        update_revision=update.revision;
        if(current_page==4 && settings_tab==3 && !overlay && !keyboard) draw_page();
    }
}
static void draw_page(void) {
    settings_content=NULL;
    settings_weight=settings_stability=NULL;
    pa_list = location_select = NULL;
    memset(quick_swatches, 0, sizeof(quick_swatches));
    rendering = true;
    scale_readout = tag_readout = inventory_grid = write_list = write_panel = NULL;
    lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_scroll_to(body, 0, 0, LV_ANIM_OFF);
    keyboard_close(NULL);
    close_overlay(NULL);
    lv_obj_clean(body);
    const char *en[] = {tr("spoolbuddy.nav.dashboard"), tr("spoolbuddy.nav.ams"), tr("spoolbuddy.nav.writeTag"), tr("spoolbuddy.nav.inventory"), tr("spoolbuddy.nav.settings")};
    for (int i = 0; i < 5; i++) {
        lv_obj_set_style_bg_color(navs[i], lv_color_hex(i == current_page ? BG : SECONDARY), 0);
        lv_label_set_text(nav_labels[i], en[i]);
        lv_obj_set_style_text_color(nav_labels[i],
                                    lv_color_hex(i == current_page ? GREEN : 0x979da4), 0);
        lv_image_set_src(nav_icons[i], i == current_page ? nav_green[i] : nav_gray[i]);
    }
    lv_label_set_text(status, feedback[0] ? feedback : tr("spoolbuddy.status.systemReady"));
    lv_obj_set_style_text_color(status, lv_color_hex(feedback_error ? 0xef4444 : GREEN), 0);
    lv_obj_set_style_bg_color(led, lv_color_hex(GREEN), 0);
    lv_label_set_text(presence, preview_online ? tr("spoolbuddy.status.backend") : tr("spoolbuddy.status.offline"));
    lv_obj_set_style_bg_color(presence_dot, lv_color_hex(preview_online ? GREEN : 0x85909d), 0);
    if (current_page == 0 && !preview_online) {
        lv_label_set_text(status,
                          tr("port.SpoolbuddyDeviceDisconnected"));
        lv_obj_set_style_text_color(status, lv_color_hex(0xa1a1aa), 0);
        lv_obj_set_style_bg_color(led, lv_color_hex(0xf59e0b), 0);
    }
    if (current_page == 0)
        dashboard();
    else if (current_page == 1)
        ams();
    else if (current_page == 2)
        write_page();
    else if (current_page == 3)
        inventory();
    else
        settings();
    rendering = false;
}
void original_init(bool zh) {
    sb_display_start();
    sb_display_lock();
    chinese = zh;
    cjk = sb_cjk_14;
    cjk.fallback = &lv_font_montserrat_20;
    regular = sb_inter_14;
    regular.fallback = &cjk;
    small = sb_inter_12;
    small.fallback = &cjk;
    heading = sb_inter_20;
    cjk_heading = sb_cjk_18;
    cjk_heading.fallback = &cjk;
    heading.fallback = &cjk_heading;
    lv_obj_t *screen = lv_screen_active();
    style(screen, BG, 0);
    lv_obj_t *top = rect(screen, 0, 0, 800, 49, SECONDARY, 0);
    border(top, BORDER, 1);
    lv_obj_t *handle = button(top, "", 0, 0, 800, 6, SECONDARY, quick_menu, 0);
    rect(handle, 386, 2, 28, 2, 0x52525b, 2);
    lv_obj_t *logo = image(top, &sb_logo, 14, 13);
    lv_image_set_scale(logo, 180);
    lv_image_set_pivot(logo, 0, 0);
    lv_obj_t *printer = lv_dropdown_create(top);
    printer_select = printer;
    lv_obj_set_pos(printer, 288, 9);
    lv_obj_set_size(printer, 180, 35);
    style(printer, BG, 3);
    border(printer, BORDER, 1);
    lv_obj_set_style_pad_left(printer, 18, 0);
    lv_obj_set_style_pad_top(printer, 8, 0);
    lv_dropdown_set_options(printer,
                            sb_live.enabled ? tr("port.LoadingPrinters") : "P1S");
    lv_obj_add_event_cb(printer, printer_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_dropdown_set_symbol(printer, &sb_icon_chevron);
    /* Match the upstream compact, right-aligned status cluster. Intrinsic label
     * widths avoid empty columns for Chinese and still fit the 12-hour clock. */
    lv_obj_t *indicators=rect(top,0,0,LV_SIZE_CONTENT,30,SECONDARY,0);
    lv_obj_set_flex_flow(indicators,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(indicators,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(indicators,8,0);
    lv_obj_align(indicators,LV_ALIGN_RIGHT_MID,-16,3);
    network_icon=image(indicators,network_connected?&sb_icon_signal:&sb_icon_wifioff,0,0);
    presence_dot = circle(indicators,0,0,10,0x85909d);
    presence = text(indicators,"",0,0,LV_SIZE_CONTENT,0x9ca3af,&small);
    clock_label = text(indicators,"",0,0,LV_SIZE_CONTENT,0x9ca3af,&small);
    update_clock(NULL);
    lv_timer_create(update_clock, 1000, NULL);
    body = rect(screen, 0, 49, 800, 348, BG, 0);
    lv_obj_t *statusbar = rect(screen, 0, 397, 800, 34, SECONDARY, 0);
    border(statusbar, BORDER, 1);
    led = circle(statusbar, 11, 11, 12, GREEN);
    status = text(statusbar, "", 31, 10, 750, GREEN, &small);
    for (int i = 0; i < 5; i++) {
        navs[i] = button(screen, "", i * 160, 431, 160, 49, SECONDARY, nav_cb, i);
        lv_obj_set_style_radius(navs[i], 0, 0);
        nav_icons[i] = image(navs[i], nav_gray[i], 68, 5);
        nav_labels[i] = center_text(navs[i], "", 0, 33, 160, MUTED, &small);
    }
    draw_page();
    sb_display_unlock();
}
void original_page(int page) {
    sb_display_lock();
    current_page = page < 0 ? 0 : page > 4 ? 4 : page;
    draw_page();
    sb_display_unlock();
}
void original_subpage(int tab) {
    sb_display_lock();
    if (current_page == 2)
        write_tab = tab < 0 ? 0 : tab > 2 ? 2 : tab;
    if (current_page == 4)
        settings_tab = tab < 0 ? 0 : tab > 4 ? 4 : tab;
    draw_page();
    sb_display_unlock();
}
void original_language(bool zh) {
    sb_display_lock();
    chinese = zh;
    if(feedback_key[0])snprintf(feedback,sizeof(feedback),"%s",tr(feedback_key));
    draw_page();
    sb_display_unlock();
}
void original_online(bool online) {
    sb_display_lock();
    preview_online = online;
    draw_page();
    sb_display_unlock();
}

void original_form(int mode) {
    sb_display_lock();
    full_form = mode > 0;
    pa_profile = mode == 2;
    draw_page();
    sb_display_unlock();
}
void original_status(const char *message) {
    if (!message)
        return;
    if (!strcmp(message, "Connected to Bambuddy"))
        message = tr("spoolbuddy.status.systemReady");
    else if (!strcmp(message, "Connecting Wi-Fi..."))
        message = tr("port.ConnectingWiFi");
    else if (!strcmp(message, "SD configuration invalid"))
        message = tr("port.InvalidSdConfiguration");
    const char *key=sb_i18n_key(message);
    snprintf(feedback_key,sizeof(feedback_key),"%s",key?key:"");
    snprintf(feedback, sizeof(feedback), "%s", key?tr(key):message);
    feedback_error = false;
    if (status)
        lv_label_set_text(status, feedback);
}
void original_command_result(const char *method, const char *path, int code, const cJSON *reply) {
    ams_command_result(path, code, reply);
    if (!strcmp(method, "PATCH") && strstr(path, "/link-tag") && code >= 200 && code < 300 &&
        sb_live.uid[0] && !strcmp(str(reply, "tag_uid"), sb_live.uid))
        ui_spool(sb_live.uid, (int)number(reply, "id", 0), str(reply, "brand"),
                 str(reply, "material"),
                 fmax(0, number(reply, "label_weight", 0) - number(reply, "weight_used", 0)),
                 number(reply, "core_weight", 250));
    if (!create_waiting || strcmp(method, "POST") || strcmp(path, create_path))
        return;
    create_waiting = false;
    const cJSON *created = cJSON_IsArray(reply) ? reply : prop(reply, "created");
    int id = (int)number(cJSON_IsArray(created) ? cJSON_GetArrayItem(created, 0) : reply, "id", 0);
    if (create_quantity > 1 && code >= 200 && code < 300 && cJSON_IsArray(created))
        snprintf(result_note, sizeof(result_note),
                 tr("port.CreatedDOfDSpools"),
                 cJSON_GetArraySize(created), create_quantity);
    if (code < 200 || code >= 300 || id <= 0)
        return;
    selected_spool = id;
    if (quick_tag_create) {
        if (strstr(create_namespace, "spoolman")) {
            char path[160];
            snprintf(path, sizeof(path), "%s/%d/link-tag", create_namespace, id);
            cJSON *tag = cJSON_CreateObject();
            cJSON_AddStringToObject(tag, "tag_uid", quick_tag_uid);
            bridge_submit("PATCH", path, tag);
            cJSON_Delete(tag);
        }
        ui_spool(quick_tag_uid, id, str(reply, "brand"), str(reply, "material"),
                 fmax(0, number(reply, "label_weight", 1000) - number(reply, "weight_used", 0)),
                 number(reply, "core_weight", 250));
        quick_tag_create = false;
    }
    if (cJSON_GetArraySize(create_profiles)) {
        char destination[160];
        snprintf(destination, sizeof(destination), "%s/%d/k-profiles", create_namespace, id);
        if (!bridge_submit("PUT", destination, create_profiles))
            original_status(
                tr("port.SpoolCreatedPaProfilesWereNotSaved"));
    }
}
void original_feedback(bool success, int code, const cJSON *reply) {
    char value[256];
    const char *detail = str(reply, "detail");
    if (!detail[0])
        detail = str(reply, "message");
    if (success && !strcmp(str(reply, "type"), "spoolbuddy_tag_written"))
        snprintf(value, sizeof(value), "%s", tr("spoolbuddy.writeTag.writeSuccess"));
    else if (success)
        snprintf(value, sizeof(value), "%s",
                 result_note[0] ? result_note : tr("port.RequestAccepted"));
    else
        snprintf(value, sizeof(value), "%s (%d) %.160s", tr("port.OperationFailed"), code,
                 detail);
    result_note[0] = 0;
    original_status(value);
    feedback_error = !success;
    if (status)
        lv_obj_set_style_text_color(status, lv_color_hex(success ? GREEN : 0xef4444), 0);
    if (success)
        bridge_refresh();
}
void original_model_changed(void) {
    if (!sb_live.enabled || !body || rendering)
        return;
    if (catalog_list)
        catalog_render();
    if (slot_link_list)
        slot_link_render();
    if (detail_remaining)
        detail_values();
    if (location_select)
        location_choices();
    if (preset_list)
        preset_render();
    if (pa_list)
        pa_render();
    if (quick_list)
        quick_render();
    if (diagnostic_output) {
        const cJSON *result = bridge_model("diagnostic");
        if (cJSON_IsObject(result))
            lv_label_set_text(diagnostic_output, str(result, "output"));
    }
    if (printer_select) {
        char options[1024] = "";
        const cJSON *p;
        int i = 0, chosen = 0;
        cJSON_ArrayForEach(p, bridge_model("printers")) {
            if (strlen(options) + strlen(str(p, "name")) + 2 >= sizeof(options))
                break;
            if (i)
                strcat(options, "\n");
            strcat(options, str(p, "name"));
            if ((int)number(p, "id", 0) == bridge_printer_id())
                chosen = i;
            i++;
        }
        updating_printers = true;
        lv_dropdown_set_options(printer_select,
                                options[0] ? options : tr("port.NoPrintersOnline"));
        lv_dropdown_set_selected(printer_select, chosen);
        updating_printers = false;
    }
    if (overlay || keyboard || (current_page == 2 && write_tab == 1) ||
        (current_page == 4 && settings_tab == 0))
        return;
    int y = lv_obj_get_scroll_y(body);
    int settings_y = settings_content ? lv_obj_get_scroll_y(settings_content) : 0;
    draw_page();
    lv_obj_scroll_to_y(body, y, LV_ANIM_OFF);
    if(settings_content) lv_obj_scroll_to_y(settings_content,settings_y,LV_ANIM_OFF);
}
void original_live_changed(void) {
    static int last_spool;
    static char last_uid[33];
    if (!sb_live.enabled)
        return;
    bool tag_changed = strcmp(last_uid, sb_live.uid) != 0;
    bool changed = last_spool != sb_live.spool_id || tag_changed;
    if (tag_changed) {
        selected_spool = sb_live.spool_id;
        close_overlay(NULL);
    }
    last_spool = sb_live.spool_id;
    snprintf(last_uid, sizeof(last_uid), "%s", sb_live.uid);
    if (changed && !rendering && !overlay && !keyboard && current_page == 0)
        draw_page();
    if(settings_weight) {
        char value[40]="— g";
        if(valid_weight())snprintf(value,sizeof(value),"%.1f g",(double)gross());
        lv_label_set_text(settings_weight,value);
        lv_obj_set_style_bg_color(settings_stability,lv_color_hex(sb_live.stable?GREEN:0xf59e0b),0);
    }
    if (scale_readout) {
        char value[48] = "—";
        if (valid_weight())
            snprintf(value, sizeof(value), "%.0fg", (double)gross());
        lv_label_set_text(scale_readout, value);
    }
    if (detail_remaining)
        detail_values();
    if (tag_readout)
        lv_label_set_text(tag_readout, sb_live.uid[0] ? tr("spoolbuddy.dashboard.tagDetected")
                                                      : tr("spoolbuddy.dashboard.noTag"));
}

static int assignment_printer, calibration_step;
static int32_t calibration_zero;

static lv_obj_t *modal_panel(const char *title, int height) {
    close_overlay(NULL);
    overlay = rect(lv_screen_active(), 0, 0, 800, 480, 0x000000, 0);
    lv_obj_set_style_bg_opa(overlay, 180, 0);
    lv_obj_t *panel = rect(overlay, 140, (480 - height) / 2, 520, height, SECONDARY, 12);
    border(panel, BORDER, 1);
    text(panel, title, 20, 16, 444, TEXT, &heading);
    button(panel, "×", 466, 10, 34, 34, SECONDARY, close_overlay, 0);
    return panel;
}
#include "ui_form_catalog.h"

static void unknown_add_send(lv_event_t *e) {
    (void)e;
    if (create_waiting || !sb_live.uid[0])
        return;
    snprintf(quick_tag_uid, sizeof(quick_tag_uid), "%s", sb_live.uid);
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "material", "PLA");
    cJSON_AddNumberToObject(p, "label_weight", 1000);
    cJSON_AddNumberToObject(p, "weight_used", 0);
    if (!strstr(bridge_inventory(), "spoolman")) {
        cJSON_AddNumberToObject(p, "core_weight", 250);
        cJSON_AddStringToObject(p, "tag_uid", quick_tag_uid);
        cJSON_AddStringToObject(p, "tag_type", "generic");
        cJSON_AddStringToObject(p, "data_origin", "spoolbuddy");
    }
    if (valid_weight())
        cJSON_AddNumberToObject(p, "last_scale_weight", round(gross()));
    create_quantity = 1;
    quick_tag_create = true;
    cJSON_Delete(create_profiles);
    create_profiles = NULL;
    snprintf(create_namespace, sizeof(create_namespace), "%s", bridge_inventory());
    snprintf(create_path, sizeof(create_path), "%s", create_namespace);
    if (bridge_submit("POST", create_path, p)) {
        create_waiting = true;
        close_overlay(NULL);
    }
    cJSON_Delete(p);
}
static void unknown_add_dialog(void) {
    lv_obj_t *panel = modal_panel(tr("spoolbuddy.modal.addToInventory"), 320);
    text(panel,
         tr("port.ForBestResultsAddTheSpoolInBambuddyFirstThenAssignThisNfcTag"),
         20, 66, 480, 0xd6b65c, &regular);
    text(panel,
         tr("port.ThisCreatesABasicPlaSpoolEditItsDetailsLaterInBambuddy"),
         20, 140, 480, MUTED, &regular);
    button(panel, tr("spoolbuddy.writeTag.cancel"), 20, 252, 232, 44, CARD, close_overlay, 0);
    button(panel, tr("spoolbuddy.modal.addAnyway"), 268, 252, 232, 44, GREEN, unknown_add_send, 0);
}

static void color_choose(lv_event_t *e) {
    if (!sb_live.enabled)
        return;
    if (!draft)
        draft_reset();
    char color[9];
    uintptr_t value = (uintptr_t)lv_event_get_user_data(e);
    if (value == UINTPTR_MAX)
        snprintf(color, sizeof(color), "00000000");
    else
        snprintf(color, sizeof(color), "%06lXFF", (unsigned long)(value & 0xffffffu));
    for (int i = 0; i < 13; i++)
        if ((i == 12 && value == UINTPTR_MAX) || (i < 12 && quick_colors[i] == value)) {
            cJSON_DeleteItemFromObjectCaseSensitive(draft, "color_name");
            cJSON_AddStringToObject(draft, "color_name", quick_names[i]);
            break;
        }
    cJSON_DeleteItemFromObjectCaseSensitive(draft, "rgba");
    cJSON_AddStringToObject(draft, "rgba", color);
    draw_page();
}
static void catalog_color_choose(lv_event_t *e) {
    const cJSON *entry =
        cJSON_GetArrayItem(bridge_model("colors"), (int)(intptr_t)lv_event_get_user_data(e));
    if (!entry)
        return;
    if (!draft)
        draft_reset();
    const char *keys[] = {"rgba", "color_name", "extra_colors", "effect_type"};
    const char *source[] = {"hex_color", "color_name", "extra_colors", "effect_type"};
    for (int i = 0; i < 4; i++) {
        cJSON_DeleteItemFromObjectCaseSensitive(draft, keys[i]);
        const char *value = str(entry, source[i]);
        if (i == 0 && value[0] == '#')
            value++;
        if (value[0]) {
            if (i == 0 && strlen(value) == 6) {
                char rgba[9];
                snprintf(rgba, sizeof(rgba), "%sFF", value);
                cJSON_AddStringToObject(draft, keys[i], rgba);
            } else
                cJSON_AddStringToObject(draft, keys[i], value);
        }
    }
    draw_page();
}
static void catalog_render(void) {
    if (!catalog_list)
        return;
    lv_obj_clean(catalog_list);
    const cJSON *colors = bridge_model("colors"), *entry;
    if (!colors) {
        text(catalog_list, tr("port.LoadingCatalog"), 8, 24, 450, MUTED,
             &small);
        return;
    }
    if (!cJSON_IsArray(colors)) {
        text(catalog_list,
             tr("port.CatalogUnavailableCommonColorsRemainAvailable"),
             8, 24, 450, MUTED, &small);
        return;
    }
    int index = 0, row = 0;
    cJSON_ArrayForEach(entry, colors) {
        if (contains(str(entry, "color_name"), catalog_query) ||
            contains(str(entry, "manufacturer"), catalog_query)) {
            char caption[180];
            snprintf(caption, sizeof(caption), "%s · %s", str(entry, "manufacturer"),
                     str(entry, "color_name"));
            lv_obj_t *b = button(catalog_list, caption, 0, row++ * 44, 476, 39, CARD,
                                 catalog_color_choose, index);
            lv_obj_set_style_text_align(b, LV_TEXT_ALIGN_LEFT, 0);
        }
        index++;
    }
    if (!row)
        text(catalog_list, tr("port.NoCatalogColorsMatch"), 8, 24, 450, MUTED,
             &small);
}
static void catalog_search_changed(lv_event_t *e) {
    snprintf(catalog_query, sizeof(catalog_query), "%s",
             lv_textarea_get_text(lv_event_get_target_obj(e)));
    catalog_render();
}
static bool url_encode(const char *value, char *out, size_t size) {
    const char hex[] = "0123456789ABCDEF";
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        bool plain = isalnum(*p) && *p < 128;
        if (n + (plain ? 1 : 3) >= size)
            return false;
        if (plain)
            out[n++] = (char)*p;
        else {
            out[n++] = '%';
            out[n++] = hex[*p >> 4];
            out[n++] = hex[*p & 15];
        }
    }
    out[n] = 0;
    return true;
}
static void color_picker(void) {
    lv_obj_t *panel = modal_panel(tr("inventory.commonColors"), sb_live.enabled ? 430 : 290);
    const unsigned colors[] = {0x000000, 0xffffff, 0x808080, 0xff0000, 0xff8000, 0xffdd00,
                               0x00ae42, 0x0066ff, 0x8800ff, 0xff88cc, 0x884400, 0xc0c0c0,
                               0xdddddd, 0x00ffff, 0x008080, 0x800000};
    for (unsigned i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
        lv_obj_t *swatch = button(panel, "", 20 + (i % 8) * 60, 72 + (i / 8) * 70, 48, 48,
                                  colors[i], color_choose, colors[i]);
        border(swatch, BORDER, 1);
    }
    if (sb_live.enabled) {
        catalog_query[0] = 0;
        char manufacturer[256], material[192], path[512];
        bool queued = false;
        if (url_encode(str(draft, "brand"), manufacturer, sizeof(manufacturer)) &&
            url_encode(str(draft, "material"), material, sizeof(material))) {
            snprintf(path, sizeof(path), "/inventory/colors/search?manufacturer=%s&material=%s",
                     manufacturer, material);
            queued = bridge_lookup("colors", path);
        }
        lv_obj_t *search =
            field(panel, tr("port.SearchCatalogColors"), 20, 201, 480, 31);
        lv_obj_set_user_data(search, (void *)"catalog_search");
        lv_obj_add_event_cb(search, catalog_search_changed, LV_EVENT_VALUE_CHANGED, NULL);
        catalog_list = rect(panel, 20, 241, 480, 170, SECONDARY, 0);
        lv_obj_add_flag(catalog_list, LV_OBJ_FLAG_SCROLLABLE);
        if (queued)
            catalog_render();
        else
            text(catalog_list, tr("port.ConnectToLoadCatalogColors"), 8, 24,
                 450, MUTED, &small);
    }
}
static void diagnostic_show(void) {
    lv_obj_t *panel = modal_panel(tr("port.DiagnosticResult"), 380);
    lv_obj_t *scroll = rect(panel, 16, 62, 488, 298, CARD, 6);
    lv_obj_add_flag(scroll, LV_OBJ_FLAG_SCROLLABLE);
    diagnostic_output = text(
        scroll,
        tr("port.WaitingForDeviceResultUpTo60Seconds"),
        12, 12, 464, MUTED, &small);
}
#ifdef SB_ENABLE_ACTION_CONFIRMATION
static void close_dialog_host(void) { close_overlay(NULL); }
#endif
static void command_sent(const char *path) {
    if (!strcmp(path, create_path))
        create_waiting = true;
    close_overlay(NULL);
    if (strstr(path, "/diagnostics/") && strstr(path, "/run?"))
        diagnostic_show();
    original_status(tr("port.SendingRequest"));
}
static void confirm_request(const char *title, const char *method, const char *path,
                            const cJSON *body) {
#ifdef SB_ENABLE_ACTION_CONFIRMATION
    lv_obj_t *panel = modal_panel(title, 230);
    sb_confirm_open(panel, method, path, body, chinese, close_dialog_host, command_sent);
#else
    (void)title;
    if (bridge_submit(method, path, body))
        command_sent(path);
#endif
}
#ifdef SB_ENABLE_MANUAL_WEIGHT
static void manual_close_host(void) { close_overlay(NULL); }
static void manual_dialog(void) {
    lv_obj_t *panel = modal_panel(tr("port.GrossWeightG"), 220);
    sb_manual_weight_open(panel, &regular, chinese, manual_close_host);
}
#endif
static void assign_to(int ams, int tray) {
    if (selected_spool <= 0) {
        original_status(tr("port.SelectASpoolFirst"));
        return;
    }
    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(
        p, strstr(bridge_inventory(), "spoolman") ? "spoolman_spool_id" : "spool_id",
        selected_spool);
    cJSON_AddNumberToObject(p, "printer_id", assignment_printer);
    cJSON_AddNumberToObject(p, "ams_id", ams);
    cJSON_AddNumberToObject(p, "tray_id", tray);
    confirm_request(tr("port.AssignToAms"), "POST",
                    strstr(bridge_inventory(), "spoolman") ? "/spoolman/inventory/slot-assignments"
                                                           : "/inventory/assignments",
                    p);
    cJSON_Delete(p);
}
static void choose_slot(lv_event_t *e) {
    int code = (int)(intptr_t)lv_event_get_user_data(e);
    assign_to(code / 256, code % 256);
}
static void choose_spool(lv_event_t *e) {
    selected_spool = (int)(intptr_t)lv_event_get_user_data(e);
    if (picker_mode == ACT_LINK) {
        cJSON *p = cJSON_CreateObject();
        if (!sb_live.uid[0]) {
            cJSON_Delete(p);
            original_status(tr("port.ScanATagFirst"));
            return;
        }
        cJSON_AddStringToObject(p, "tag_uid", sb_live.uid);
        if (sb_live.tray_uuid[0])
            cJSON_AddStringToObject(p, "tray_uuid", sb_live.tray_uuid);
        char path[192];
        snprintf(path, sizeof(path), "%s/%d/link-tag", bridge_inventory(), selected_spool);
        confirm_request(tr("port.LinkTagToSpool"), "PATCH", path, p);
        cJSON_Delete(p);
        picker_mode = 0;
        return;
    }
    assign_to(selected_slot / 256, selected_slot % 256);
}
static void assign_picker(void) {
    assignment_printer = bridge_printer_id();
    lv_obj_t *panel = modal_panel(tr("port.ChooseAmsSlot"), 360);
    lv_obj_t *list = rect(panel, 16, 62, 488, 278, SECONDARY, 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    const cJSON *unit, *tray;
    int index = 0;
    cJSON_ArrayForEach(unit, prop(bridge_model("printer_status"), "ams"))
        cJSON_ArrayForEach(tray, prop(unit, "tray")) {
        int ams = number(unit, "id", 0), id = number(tray, "id", 0);
        char caption[120];
        snprintf(caption, sizeof(caption), "AMS %c%d   %s", 'A' + ams, id + 1,
                 str(tray, "tray_type"));
        button(list, caption, 0, index++ * 51, 476, 44, CARD, choose_slot, ams * 256 + id);
    }
    if (!index)
        text(list, tr("port.NoAmsSlotsAvailable"), 10, 40, 460, MUTED, &regular);
}
static void slot_picker(int slot) {
    selected_slot = slot;
    picker_mode = 0;
    assignment_printer = bridge_printer_id();
    lv_obj_t *panel = modal_panel(tr("port.AssignSpool"), 360);
    lv_obj_t *list = rect(panel, 16, 62, 488, 278, SECONDARY, 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    const cJSON *s;
    int index = 0;
    cJSON_ArrayForEach(s, bridge_model("inventory")) {
        char caption[180];
        snprintf(caption, sizeof(caption), "#%d  %s %s", (int)number(s, "id", 0), str(s, "brand"),
                 str(s, "material"));
        button(list, caption, 0, index++ * 51, 476, 44, CARD, choose_spool,
               (int)number(s, "id", 0));
    }
}
#include "ui_ams_config.h"

#include "ui_inventory_detail.h"

static void calibration_next(lv_event_t *e) {
    (void)e;
    if (!sb_live.raw_valid) {
        original_status(tr("port.ScaleAdcIsNotReady"));
        return;
    }
    if (calibration_step == 0) {
        calibration_zero = sb_live.raw;
        calibration_step = 1;
        lv_obj_t *panel = modal_panel(tr("port.CalibrateScale"), 250);
        text(panel,
             tr("port.PlaceAKnownWeightThenEnterItsGrams"),
             20, 62, 480, MUTED, &regular);
        manual_input = field(panel, "", 20, 103, 232, 46);
        lv_textarea_set_accepted_chars(manual_input, "0123456789.");
        lv_obj_set_user_data(manual_input, (void *)"calibration_weight");
        button(panel, tr("spoolbuddy.weight.calibrate"), 270, 103, 230, 46, GREEN, calibration_next, 0);
        return;
    }
    float grams;
    if (!sb_parse_weight(lv_textarea_get_text(manual_input), &grams) || grams <= 0 ||
        sb_live.raw == calibration_zero) {
        original_status(tr("port.CheckKnownWeightAndAdcReading"));
        return;
    }
    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(p, "known_weight_grams", grams);
    cJSON_AddNumberToObject(p, "raw_adc", sb_live.raw);
    cJSON_AddNumberToObject(p, "tare_raw_adc", calibration_zero);
    char path[192];
    snprintf(path, sizeof(path), "/spoolbuddy/devices/%s/calibration/set-factor", bridge_device());
    keyboard_close(NULL);
    confirm_request(tr("port.SaveScaleCalibration"), "POST", path, p);
    cJSON_Delete(p);
}
static void action_cb(lv_event_t *e) {
    if (!sb_live.enabled)
        return;
    int action = (int)(intptr_t)lv_event_get_user_data(e);
    if (action >= 200000) {
        ams_slot_actions(action - 200000);
        return;
    }
    if (action == 700) {
        color_picker();
        return;
    }
    if (action == 0) {
#ifdef SB_ENABLE_MANUAL_WEIGHT
        manual_dialog();
#endif
        return;
    }
    if (action == ACT_ASSIGN) {
        assign_picker();
        return;
    }
    if (action == ACT_LINK) {
        slot_picker(0);
        picker_mode = ACT_LINK;
        return;
    }
    if (action == ACT_UNKNOWN_ADD) {
        unknown_add_dialog();
        return;
    }
    if (action == ACT_CREATE) {
        if (current_page != 2 || write_tab != 1) {
            current_page = 2;
            write_tab = 1;
            draft_reset();
            draw_page();
            return;
        }
        quick_tag_create = false;
        cJSON *p = draft_payload();
        if (!p)
            return;
        if (create_waiting) {
            cJSON_Delete(p);
            return;
        }
        cJSON_Delete(create_profiles);
        create_profiles = cJSON_Duplicate(selected_profiles, true);
        snprintf(create_namespace, sizeof(create_namespace), "%s", bridge_inventory());
        snprintf(create_path, sizeof(create_path), "%s%s", create_namespace,
                 create_quantity > 1 ? "/bulk" : "");
        if (create_quantity > 1) {
            cJSON *bulk = cJSON_CreateObject();
            cJSON_AddNumberToObject(bulk, "quantity", create_quantity);
            cJSON_AddItemToObject(bulk, "spool", p);
            p = bulk;
        }
        confirm_request(tr("port.CreateSpool"), "POST", create_path, p);
        cJSON_Delete(p);
        return;
    }
    if (action == ACT_TARE) {
        sb_live.tare_pending = true;
        return;
    }
    if (action == ACT_CALIBRATE) {
        calibration_step = 0;
        lv_obj_t *panel = modal_panel(tr("port.CalibrateScale"), 230);
        text(panel, tr("port.RemoveEverythingFromTheScale"), 20, 78, 480, MUTED,
             &regular);
        button(panel, tr("spoolbuddy.settings.setZero"), 20, 158, 480, 48, GREEN, calibration_next, 0);
        return;
    }
    if (action == ACT_SAVE_CONFIG) {
        original_status(tr("port.ConfigurationStoredOnDevice"));
        return;
    }
    int id = current_page == 0 && !overlay ? sb_live.spool_id
             : selected_spool > 0          ? selected_spool
                                           : sb_live.spool_id;
    char path[192];
    cJSON *p = cJSON_CreateObject();
    const char *method = "POST", *title = tr("port.ConfirmOperation");
    if (action == ACT_SYNC) {
        if (id <= 0 || !valid_weight()) {
            original_status(tr("port.SelectASpoolAndEnterWeightFirst"));
            cJSON_Delete(p);
            return;
        }
        snprintf(path, sizeof(path), "/spoolbuddy/scale/update-spool-weight");
        cJSON_AddNumberToObject(p, "spool_id", id);
        cJSON_AddNumberToObject(p, "weight_grams", floor((double)gross() + 0.5));
        title = tr("port.SyncGrossWeightRoundedToGrams");
    } else if (action == ACT_WRITE) {
        if (id <= 0) {
            original_status(tr("port.SelectASpoolFirst"));
            cJSON_Delete(p);
            return;
        }
        snprintf(path, sizeof(path), "/spoolbuddy/nfc/write-tag");
        cJSON_AddStringToObject(p, "device_id", bridge_device());
        cJSON_AddNumberToObject(p, "spool_id", id);
        title = tr("port.WriteNfcTag");
    } else if (action == ACT_CANCEL_WRITE) {
        snprintf(path, sizeof(path), "/spoolbuddy/devices/%s/cancel-write", bridge_device());
    } else if (action == ACT_UNLINK) {
        snprintf(path, sizeof(path), "%s/%d/link-tag", bridge_inventory(), id);
        method = "PATCH";
        cJSON_AddStringToObject(p, "tag_uid", "");
        cJSON_AddStringToObject(p, "tray_uuid", "");
        if (!strstr(bridge_inventory(), "spoolman"))
            cJSON_AddStringToObject(p, "data_origin", "manual");
        title = tr("port.UnlinkNfcTag");
    } else if (action == 500) {
        const cJSON *a = assignment(id);
        if (!a) {
            cJSON_Delete(p);
            return;
        }
        method = "DELETE";
        if (strstr(bridge_inventory(), "spoolman"))
            snprintf(path, sizeof(path), "/spoolman/inventory/slot-assignments/%d", id);
        else
            snprintf(path, sizeof(path), "/inventory/assignments/%d/%d/%d",
                     (int)number(a, "printer_id", 0), (int)number(a, "ams_id", 0),
                     (int)number(a, "tray_id", 0));
    } else if (action == ACT_REBOOT || action == ACT_RESTART || action == ACT_SHUTDOWN ||
               action == ACT_RESTART_BROWSER) {
        snprintf(path, sizeof(path), "/spoolbuddy/devices/%s/system/command", bridge_device());
        cJSON_AddStringToObject(p, "command",
                                action == ACT_REBOOT            ? "reboot"
                                : action == ACT_RESTART         ? "restart_daemon"
                                : action == ACT_RESTART_BROWSER ? "restart_browser"
                                                                : "shutdown");
    } else if (action >= ACT_DIAG_NFC && action <= ACT_DIAG_TAG) {
        const char *kind = action == ACT_DIAG_NFC     ? "nfc"
                           : action == ACT_DIAG_SCALE ? "scale"
                                                      : "read_tag";
        snprintf(path, sizeof(path), "/spoolbuddy/diagnostics/%s/run?diagnostic=%s",
                 bridge_device(), kind);
    } else if (action == ACT_CHECK_UPDATE || action == ACT_UPDATE_SD) {
        bool accepted=action==ACT_UPDATE_SD ? sb_update_request_sd() : sb_update_request();
        original_status(accepted?tr("port.UpdateRequestQueued"):
                                 tr("port.UpdateAlreadyRunningOrUnavailable"));
        draw_page();
        cJSON_Delete(p);
        return;
    } else {
        original_status(tr("port.BackendCapabilityIsNotAvailableYet"));
        cJSON_Delete(p);
        return;
    }
    if (!strcmp(method, "GET")) {
        bridge_submit(method, path, NULL);
        cJSON_Delete(p);
        return;
    }
    if (action == ACT_REBOOT || action == ACT_RESTART || action == ACT_SHUTDOWN ||
        action == ACT_RESTART_BROWSER)
        original_confirm(title, method, path, p);
    else
        confirm_request(title, method, path, p);
    cJSON_Delete(p);
}
static void draft_reset(void) {
    cJSON_Delete(selected_profiles);
    selected_profiles = cJSON_CreateArray();
    cJSON_Delete(draft);
    draft = cJSON_CreateObject();
    cJSON_AddStringToObject(draft, "material", "PLA");
    cJSON_AddNumberToObject(draft, "label_weight", 1000);
    cJSON_AddNumberToObject(draft, "core_weight", 250);
    core_touched = false;
    cJSON_AddNumberToObject(draft, "quantity", 1);
    cJSON_AddStringToObject(draft, "rgba", "808080FF");
}
static void draft_changed(lv_event_t *e) {
    const char *key = lv_event_get_user_data(e);
    const char *value = lv_textarea_get_text(lv_event_get_target_obj(e));
    if (!strcmp(key, "core_weight")) {
        core_touched = true;
        cJSON_DeleteItemFromObjectCaseSensitive(draft, "core_weight_catalog_id");
    }
    if (!draft)
        draft_reset();
    cJSON_DeleteItemFromObjectCaseSensitive(draft, key);
    cJSON_AddStringToObject(draft, key, value);
}
static void draft_bind(lv_obj_t *field_obj, const char *key) {
    if (!sb_live.enabled)
        return;
    if (!draft)
        draft_reset();
    const cJSON *value = prop(draft, key);
    char number_value[48];
    if (cJSON_IsString(value))
        lv_textarea_set_text(field_obj, value->valuestring);
    else if (cJSON_IsNumber(value)) {
        snprintf(number_value, sizeof(number_value), "%.3f", value->valuedouble);
        lv_textarea_set_text(field_obj, number_value);
    }
    lv_obj_set_user_data(field_obj, (void *)key);
    lv_obj_add_event_cb(field_obj, draft_changed, LV_EVENT_VALUE_CHANGED, (void *)key);
}
static cJSON *draft_payload(void) {
    if (!draft)
        draft_reset();
    if (!str(draft, "material")[0] ||
        (full_form && !quick_add && !strstr(bridge_inventory(), "spoolman") &&
         (!str(draft, "slicer_filament")[0] || !str(draft, "brand")[0] ||
          !str(draft, "subtype")[0]))) {
        original_status(tr("port.CompleteTheRequiredFilamentFields"));
        return NULL;
    }
    cJSON *p = cJSON_Duplicate(draft, true);
    if (!p)
        return NULL;
    const char *numeric[] = {"label_weight",    "core_weight", "remaining_weight",
                             "measured_weight", "cost_per_kg", "low_stock_threshold_pct",
                             "quantity"};
    for (unsigned i = 0; i < sizeof(numeric) / sizeof(numeric[0]); i++) {
        cJSON *v = cJSON_GetObjectItemCaseSensitive(p, numeric[i]);
        if (cJSON_IsString(v)) {
            if (!v->valuestring[0]) {
                cJSON_DeleteItemFromObjectCaseSensitive(p, numeric[i]);
                continue;
            }
            float grams;
            if (!sb_parse_weight(v->valuestring, &grams)) {
                cJSON_Delete(p);
                original_status(tr("port.InvalidNumericValue"));
                return NULL;
            }
            cJSON_ReplaceItemInObjectCaseSensitive(p, numeric[i], cJSON_CreateNumber(grams));
        }
    }
    double quantity = number(p, "quantity", 1);
    if (quantity < 1 || quantity > 100 || floor(quantity) != quantity) {
        cJSON_Delete(p);
        original_status(tr("port.QuantityMustBe1To100"));
        return NULL;
    }
    create_quantity = full_form && quick_add ? (int)quantity : 1;
    cJSON_DeleteItemFromObjectCaseSensitive(p, "quantity");
    const char *rgba = str(p, "rgba");
    if (rgba[0] == '#')
        rgba++;
    if (strlen(rgba) != 6 && strlen(rgba) != 8) {
        cJSON_Delete(p);
        original_status(tr("port.UseA6Or8DigitHexColor"));
        return NULL;
    }
    for (size_t i = 0; rgba[i]; i++)
        if (!isxdigit((unsigned char)rgba[i])) {
            cJSON_Delete(p);
            original_status(tr("port.InvalidHexColor"));
            return NULL;
        }
    char normalized[9];
    snprintf(normalized, sizeof(normalized), "%s%s", rgba, strlen(rgba) == 6 ? "FF" : "");
    cJSON_ReplaceItemInObjectCaseSensitive(p, "rgba", cJSON_CreateString(normalized));
    double label = number(p, "label_weight", 1000), core = number(p, "core_weight", 250),
           remaining = number(p, "remaining_weight", label);
    if (prop(p, "measured_weight"))
        remaining = fmax(0, number(p, "measured_weight", 0) - core);
    cJSON_DeleteItemFromObjectCaseSensitive(p, "weight_used");
    cJSON_AddNumberToObject(p, "weight_used", fmax(0, label - remaining));
    cJSON_DeleteItemFromObjectCaseSensitive(p, "remaining_weight");
    cJSON_DeleteItemFromObjectCaseSensitive(p, "measured_weight");
    if (strstr(bridge_inventory(), "spoolman")) {
        cJSON_DeleteItemFromObjectCaseSensitive(p, "core_weight_catalog_id");
        if (!core_touched)
            cJSON_DeleteItemFromObjectCaseSensitive(p, "core_weight");
    }
    return p;
}

#ifdef SB_SIMULATOR
void original_host_char(unsigned code) {
    if (!keyboard)
        return;
    lv_obj_t *target = lv_keyboard_get_textarea(keyboard);
    if (!target)
        return;
    if (code == 8)
        lv_textarea_delete_char(target);
    else if (code == 13)
        keyboard_close(NULL);
    else if (code >= 32 && code < 127) {
        char value[2] = {(char)code, 0};
        lv_textarea_add_text(target, value);
    }
}
static lv_obj_t *test_button(lv_obj_t *root, const char *caption) {
    if (lv_obj_check_type(root, &lv_button_class)) {
        for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
            lv_obj_t *child = lv_obj_get_child(root, i);
            if (lv_obj_check_type(child, &lv_label_class) &&
                !strcmp(lv_label_get_text(child), caption))
                return root;
        }
    }
    for (int i = (int)lv_obj_get_child_count(root) - 1; i >= 0; i--) {
        lv_obj_t *found = test_button(lv_obj_get_child(root, i), caption);
        if (found)
            return found;
    }
    return NULL;
}
bool original_test_click(const char *caption) {
    lv_obj_t *obj = test_button(overlay ? overlay : lv_screen_active(), caption);
    if (!obj)
        return false;
    lv_obj_send_event(obj, LV_EVENT_CLICKED, NULL);
    return true;
}
static bool test_label(lv_obj_t *root,const char *caption) {
    if(lv_obj_check_type(root,&lv_label_class) && !strcmp(lv_label_get_text(root),caption)) return true;
    for(uint32_t i=0;i<lv_obj_get_child_count(root);i++)
        if(test_label(lv_obj_get_child(root,i),caption)) return true;
    return false;
}
bool original_test_text(const char *caption) {return test_label(lv_screen_active(),caption);}
void original_test_settings_scroll(int y) {
    if(settings_content) {lv_obj_update_layout(settings_content);lv_obj_scroll_to_y(settings_content,y,LV_ANIM_OFF);}
}
static bool test_input(lv_obj_t *root, const char *key, const char *value) {
    if (lv_obj_check_type(root, &lv_textarea_class) && lv_obj_get_user_data(root) &&
        !strcmp(lv_obj_get_user_data(root), key)) {
        lv_textarea_set_text(root, value);
        return true;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++)
        if (test_input(lv_obj_get_child(root, i), key, value))
            return true;
    return false;
}
bool original_test_input(const char *key, const char *value) {
    return test_input(lv_screen_active(), key, value);
}
static lv_obj_t *test_dropdown(lv_obj_t *root, const char *key) {
    if (lv_obj_check_type(root, &lv_dropdown_class) && lv_obj_get_user_data(root) &&
        !strcmp(lv_obj_get_user_data(root), key))
        return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = test_dropdown(lv_obj_get_child(root, i), key);
        if (found)
            return found;
    }
    return NULL;
}
bool original_test_dropdown(const char *key, int index) {
    lv_obj_t *drop = test_dropdown(lv_screen_active(), key);
    if (!drop)
        return false;
    lv_dropdown_set_selected(drop, index);
    lv_obj_send_event(drop, LV_EVENT_VALUE_CHANGED, NULL);
    return true;
}
int original_test_cards(bool write) {
    lv_obj_t *list = write ? write_list : inventory_grid;
    int count = 0;
    if (list)
        for (uint32_t i = 0; i < lv_obj_get_child_count(list); i++)
            if (lv_obj_check_type(lv_obj_get_child(list, i), &lv_button_class))
                count++;
    return count;
}
static lv_obj_t *test_quick_switch(lv_obj_t *root) {
    if (lv_obj_check_type(root, &lv_switch_class) && lv_obj_get_user_data(root) &&
        !strcmp(lv_obj_get_user_data(root), "quick_add"))
        return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = test_quick_switch(lv_obj_get_child(root, i));
        if (found)
            return found;
    }
    return NULL;
}
void original_test_quickadd(bool active) {
    lv_obj_t *toggle = test_quick_switch(lv_screen_active());
    if (toggle) {
        if (active)
            lv_obj_add_state(toggle, LV_STATE_CHECKED);
        else
            lv_obj_remove_state(toggle, LV_STATE_CHECKED);
        lv_obj_send_event(toggle, LV_EVENT_VALUE_CHANGED, NULL);
    }
}
void original_test_refresh_clock(void) {update_clock(NULL);}
int original_test_update_height(void) {
    lv_obj_update_layout(lv_screen_active());
    return settings_content && lv_obj_get_child_count(settings_content)
        ? lv_obj_get_height(lv_obj_get_child(settings_content,0)):0;
}

bool original_test_network(bool connected) {
    if (!network_icon) return false;
    lv_obj_update_layout(lv_screen_active());
    lv_area_t wifi,dot,label,clock;
    lv_obj_get_coords(network_icon,&wifi);lv_obj_get_coords(presence_dot,&dot);
    lv_obj_get_coords(presence,&label);lv_obj_get_coords(clock_label,&clock);
    const lv_image_dsc_t *icon = connected ? &sb_icon_signal : &sb_icon_wifioff;
    return lv_image_get_src(network_icon) == icon && icon->header.w <= 22 && icon->header.h == 20
        && wifi.x1>468 && wifi.y1>=6 && wifi.y2<49
        && dot.x1-wifi.x2==9 && label.x1-dot.x2==9
        && clock.x1-label.x2==9 && clock.x2<=784;
}
void original_test_colors(void) { color_picker(); }
void original_test_slot(int code) { ams_slot_actions(code); }
void original_test_quick(void) {
    close_overlay(NULL);
    quick_menu(NULL);
}
void original_test_detail(int id) { detail_show(id); }
void original_test_writer(int id) {
    const cJSON *s = spool_by_id(id);
    current_page = 2;
    write_tab = s && (str(s, "tag_uid")[0] || str(s, "tray_uuid")[0]) ? 2 : 0;
    selected_spool = id;
    write_query[0] = 0;
    draw_page();
}
void original_test_close(void) { close_overlay(NULL); }
#endif
