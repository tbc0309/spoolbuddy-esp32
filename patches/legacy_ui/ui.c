// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ui.h"
#include "ui_platform.h"
#include "settings.h"
#include "screens.h"
#include "lvgl.h"
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
extern const lv_font_t sb_font_20;
static lv_font_t ui_font;
static lv_obj_t *current_title, *idle_spool;
extern const lv_image_dsc_t sb_logo;
static lv_obj_t *status_label, *tag_label, *info_label, *weight_label, *source_label, *manual_label,
    *tare_label, *sync_label, *sensor_label, *lang_label, *footer;
static lv_obj_t *modal, *input, *input_error, *keyboard;
static bool zh, manual_mode, sensor_valid, sensor_stable, manual_pending, sync_pending;
static atomic_bool tare_requested, manual_flag;
static float sensor_grams, manual_grams, core_grams, remaining_grams;
static int matched_id, sync_id, brightness = 100, blank_seconds;
static float sync_grams;
static bool blanked;
static bool local_brightness;
static char current_uid[21], status_key[160] = "Starting...",
                             detail_key[240] = "Connect to Bambuddy to manage your spools",
                             brand[80], material[80];
static const char *tr(const char *en, const char *cn) { return zh ? cn : en; }
static const char *translate(const char *key) {
    static const char *pairs[][2] = {
        {"Starting...", "正在启动…"},
        {"Request pending", "上一个请求正在处理中"},
        {"Request queued", "请求已提交，正在处理…"},
        {"Request completed", "操作已完成"},
        {"Request failed", "操作失败，请检查连接和权限"},
        {"Select a spool first", "请先选择耗材"},
        {"Select a spool in Inventory first", "请先在库存页选择耗材"},
        {"Connecting Wi-Fi...", "正在连接 Wi-Fi…"},
        {"Connected to Bambuddy", "已连接 Bambuddy"},
        {"Wi-Fi disconnected; reconnecting...", "Wi-Fi 已断开，正在重连…"},
        {"Bambuddy unavailable; retrying...", "无法连接 Bambuddy，正在重试…"},
        {"Backend disconnected; retrying...", "服务器已断开，正在重试…"},
        {"Set Wi-Fi and Bambuddy in menuconfig", "请将配置文件放入 SD 卡后重启"},
        {"SD configuration invalid", "SD 卡配置无效，请检查文件后重启"},
        {"Place an NTAG sticker to write", "请放置 NTAG 标签进行写入"},
        {"Tag write verified", "标签写入并回读验证成功"},
        {"Tag write failed", "标签写入失败"},
        {"Tare failed: scale not ready", "去皮失败：电子秤未就绪"},
        {"Scale zeroed", "电子秤已去皮"},
        {"Connect to Bambuddy to manage your spools", "连接 Bambuddy 后可管理耗材"},
        {"Ready for the next spool", "等待下一卷耗材"},
        {"Bambu / MIFARE Classic", "已检测到拓竹标签"},
        {"NFC sticker detected", "已检测到 NFC 标签"},
        {"Unknown spool: link it in Bambuddy", "未知耗材，请在 Bambuddy 中关联"},
        {"API rejected event; check key / permissions", "请求被拒绝，请检查 API key 和权限"},
        {"Manual weight applied", "已采用手动输入的整盘总重"},
        {"Weight saved to inventory", "重量已同步到库存"},
        {"Weight sync failed", "重量同步失败，请检查连接"},
        {"Connect before saving weight", "请连接服务器后再同步重量"},
        {"Scan a known spool and enter weight first", "请先识别已关联耗材并输入有效重量"},
        {"Spool details unavailable", "耗材已识别，但详情暂时无法读取"},
        {"Simulation mode: virtual peripherals", "模拟模式：虚拟 NFC 和电子秤"},
        {"Backend mode: real API requests", "后端联调模式：真实 API 请求"}};
    for (size_t i = 0; i < sizeof(pairs) / sizeof(pairs[0]); i++)
        if (!strcmp(key, pairs[i][0]))
            return zh ? pairs[i][1] : pairs[i][0];
    return key;
}
static lv_obj_t *text(lv_obj_t *parent, const char *value, int x, int y, int w) {
    lv_obj_t *o = lv_label_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_width(o, w);
    lv_label_set_text(o, value);
    return o;
}
static lv_obj_t *button(lv_obj_t *parent, int x, int y, int w, int h, lv_event_cb_t cb,
                        lv_obj_t **caption) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x262c32), 0);
    lv_obj_set_style_text_font(b, &ui_font, 0);
    lv_obj_set_style_text_color(b, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    *caption = lv_label_create(b);
    lv_obj_center(*caption);
    return b;
}
static lv_obj_t *card(int y, int h) {
    lv_obj_t *o = lv_obj_create(lv_screen_active());
    lv_obj_set_pos(o, 20, y);
    lv_obj_set_size(o, 440, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(0x262c32), 0);
    lv_obj_set_style_text_font(o, &ui_font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(0xdddddd), 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o,lv_color_hex(0x363e46),0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_radius(o, 16, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
static void render_weight(void) {
    bool valid = manual_mode || sensor_valid;
    float g = manual_mode ? manual_grams : sensor_grams;
    if (valid)
        lv_label_set_text_fmt(weight_label, "%.1f g", (double)g);
    else
        lv_label_set_text(weight_label, "-- g");
    const char *label =
        manual_mode    ? tr("Manual gross weight", "手动输入 · 总重")
        : sensor_valid ? (sensor_stable ? tr("Scale · stable", "电子秤 · 已稳定")
                                        : tr("Scale · measuring", "电子秤 · 测量中"))
                       : tr("Scale unavailable", "电子秤未就绪");
    lv_label_set_text(source_label, label);
    lv_obj_set_style_text_line_space(source_label,-8,0);
    if(idle_spool) {
        if(matched_id>0 || current_uid[0]) lv_obj_add_flag(idle_spool,LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(idle_spool,LV_OBJ_FLAG_HIDDEN);
    }
    if(current_title) lv_label_set_text(current_title,tr("CURRENT SPOOL","当前耗材"));
    lv_obj_set_pos(info_label,16,(matched_id>0 || current_uid[0])?56:138);
    if (matched_id > 0) {
        lv_obj_set_style_text_align(info_label,LV_TEXT_ALIGN_LEFT,0);
        if (valid)
            lv_label_set_text_fmt(info_label, "#%d  %s %s\n%s %.1f g\n%s %.1f g", matched_id, brand,
                                  material, tr("Core:", "空盘重："), (double)core_grams,
                                  tr("Estimated remaining:", "剩余估算："),
                                  (double)fmaxf(0, g - core_grams));
        else
            lv_label_set_text_fmt(info_label, "#%d  %s %s\n%s %.1f g", matched_id, brand, material,
                                  tr("Stored remaining:", "库存剩余："), (double)remaining_grams);
    } else if(!current_uid[0]) {
        lv_label_set_text(info_label,tr("Ready to scan\nPlace a spool on the scale to identify it", "准备扫描\n将耗材放到秤上以识别标签"));
        lv_obj_set_style_text_align(info_label,LV_TEXT_ALIGN_CENTER,0);
    } else {
        lv_label_set_text(info_label, translate(detail_key));
        lv_obj_set_style_text_align(info_label,LV_TEXT_ALIGN_LEFT,0);
    }
}
static void refresh(void) {
    lv_label_set_text(status_label, translate(status_key));
    lv_label_set_text(tag_label,
                      current_uid[0] ? current_uid : tr("Tap an NFC spool", "请靠近 NFC 耗材标签"));
    lv_label_set_text(manual_label, tr("Manual input", "手动输入"));
    lv_obj_center(manual_label);
    lv_label_set_text(tare_label, tr("Tare / Zero", "电子秤去皮"));
    lv_obj_center(tare_label);
    lv_label_set_text(sync_label, tr("Save gross weight to inventory", "将整盘总重同步到库存"));
    lv_obj_center(sync_label);
    lv_label_set_text(sensor_label, tr("Use electronic scale", "切换到电子秤"));
    lv_obj_center(sensor_label);
    lv_label_set_text(lang_label, zh ? "EN" : "中文");
    lv_obj_center(lang_label);
    lv_label_set_text(footer, tr("SD config · PN5180 · NAU7802", "SD 卡配置 · NFC · 称重"));
    screens_language(zh);
    render_weight();
}
void ui_request_tare(void) { atomic_store(&tare_requested, true); }
static void tare_cb(lv_event_t *e) {
    (void)e;
    atomic_store(&tare_requested, true);
}
static void language_cb(lv_event_t *e) {
    (void)e;
    zh = !zh;
    sb_settings.chinese = zh;
    refresh();
}
void ui_language(bool chinese) {
    sb_display_lock();
    zh = chinese;
    sb_settings.chinese = zh;
    refresh();
    sb_display_unlock();
}
static void close_modal(void) {
    if (modal) {
        lv_obj_delete(modal);
        modal = NULL;
        input = NULL;
        input_error = NULL;
        keyboard = NULL;
    }
}
static void cancel_cb(lv_event_t *e) {
    (void)e;
    close_modal();
}
static void apply_cb(lv_event_t *e) {
    (void)e;
    float grams;
    if (!sb_parse_weight(lv_textarea_get_text(input), &grams)) {
        lv_label_set_text(input_error, tr("Enter 0–100000 grams", "请输入 0 至 100000 克"));
        return;
    }
    manual_grams = grams;
    manual_mode = true;
    manual_pending = true;
    atomic_store(&manual_flag, true);
    snprintf(status_key, sizeof(status_key), "Manual weight applied");
    close_modal();
    refresh();
}
static void manual_cb(lv_event_t *e) {
    (void)e;
    if (modal)
        return;
    modal = lv_obj_create(lv_screen_active());
    lv_obj_set_pos(modal, 0, 0);
    lv_obj_set_size(modal, 800, 480);
    lv_obj_set_style_bg_color(modal, lv_color_hex(0x181c20), 0);
    lv_obj_set_style_text_color(modal, lv_color_hex(0xdddddd), 0);
    lv_obj_set_style_border_width(modal, 0, 0);
    lv_obj_set_style_pad_all(modal, 0, 0);
    lv_obj_remove_flag(modal, LV_OBJ_FLAG_SCROLLABLE);
    text(modal, tr("Manual gross weight (g)", "手动输入整盘总重（克）"), 24, 14, 740);
    text(modal, tr("Include filament AND empty spool.", "请输入耗材加空盘的总重量。"), 24, 55, 740);
    input = lv_textarea_create(modal);
    lv_obj_set_pos(input, 24, 100);
    lv_obj_set_size(input, 350, 60);
    lv_textarea_set_one_line(input, true);
    lv_textarea_set_accepted_chars(input, "0123456789.");
    lv_textarea_set_max_length(input, 12);
    char value[32] = "";
    if (manual_mode)
        snprintf(value, sizeof(value), "%.1f", (double)manual_grams);
    lv_textarea_set_text(input, value);
    input_error = text(modal, "", 24, 176, 740);
    lv_obj_set_style_text_color(input_error, lv_color_hex(0xfda4af), 0);
    lv_obj_t *cap;
    button(modal, 410, 100, 170, 60, apply_cb, &cap);
    lv_label_set_text(cap, tr("Apply", "采用重量"));
    lv_obj_center(cap);
    button(modal, 600, 100, 170, 60, cancel_cb, &cap);
    lv_label_set_text(cap, tr("Cancel", "取消"));
    lv_obj_center(cap);
    keyboard = lv_keyboard_create(modal);
    lv_obj_set_style_text_font(keyboard, &ui_font, LV_PART_ITEMS);
    lv_obj_set_align(keyboard, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(keyboard, 10, 232);
    lv_obj_set_size(keyboard, 780, 238);
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(keyboard, input);
    lv_obj_add_event_cb(keyboard, apply_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(keyboard, cancel_cb, LV_EVENT_CANCEL, NULL);
}
static void sensor_cb(lv_event_t *e) {
    (void)e;
    manual_mode = false;
    atomic_store(&manual_flag, false);
    render_weight();
}
static void sync_cb(lv_event_t *e) {
    (void)e;
    if (matched_id <= 0 || (!manual_mode && !sensor_valid)) {
        snprintf(status_key, sizeof(status_key), "Scan a known spool and enter weight first");
        refresh();
        return;
    }
    sync_id = matched_id;
    sync_grams = manual_mode ? manual_grams : sensor_grams;
    sync_pending = true;
}
static void idle(lv_timer_t *t) {
    (void)t;
    bool should =
        blank_seconds > 0 && lv_display_get_inactive_time(NULL) >= (uint32_t)blank_seconds * 1000;
    if (should != blanked) {
        blanked = should;
        sb_display_brightness(blanked ? 0 : brightness);
    }
}
void ui_init(void) {
    sb_display_start();
    sb_display_lock();
    zh = sb_settings.chinese;
    ui_font = sb_font_20;
    ui_font.fallback = &lv_font_montserrat_20;
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x181c20), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xdddddd), 0);
    lv_obj_set_style_text_font(screen, &ui_font, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *top = lv_obj_create(screen);
    lv_obj_set_pos(top,0,0); lv_obj_set_size(top,800,48);
    lv_obj_set_style_bg_color(top,lv_color_hex(0x262c32),0);
    lv_obj_set_style_border_width(top,0,0); lv_obj_set_style_radius(top,0,0);
    lv_obj_t *logo=lv_image_create(screen);
    lv_image_set_src(logo,&sb_logo); lv_obj_set_pos(logo,16,6);
    button(screen, 574, 4, 70, 40, language_cb, &lang_label);
    lv_obj_t *left=card(96,126);
    lv_obj_set_pos(left,16,96); lv_obj_set_size(left,304,126);
    source_label=text(left,"",12,4,280);
    lv_obj_set_style_text_line_space(source_label,-8,0);
    weight_label=text(left,"-- g",12,43,280);
    lv_obj_set_style_text_font(weight_label,&lv_font_montserrat_32,0);
    tag_label=text(left,"",12,82,280);
    lv_obj_set_style_text_line_space(tag_label,-8,0);
    lv_obj_set_style_text_color(tag_label,lv_color_hex(0x999999),0);
    lv_obj_t *right=card(96,232);
    lv_obj_set_pos(right,336,96); lv_obj_set_size(right,448,232);
    current_title=text(right,tr("CURRENT SPOOL","当前耗材"),16,6,416);
    idle_spool=lv_obj_create(right); lv_obj_set_pos(idle_spool,181,42); lv_obj_set_size(idle_spool,86,86);
    lv_obj_set_style_radius(idle_spool,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_bg_color(idle_spool,lv_color_hex(0x00ae42),0);
    lv_obj_set_style_border_color(idle_spool,lv_color_hex(0x97d4a7),0); lv_obj_set_style_border_width(idle_spool,6,0);
    lv_obj_t *hub=lv_obj_create(idle_spool); lv_obj_set_size(hub,26,26); lv_obj_center(hub);
    lv_obj_set_style_radius(hub,LV_RADIUS_CIRCLE,0); lv_obj_set_style_bg_color(hub,lv_color_hex(0x181c20),0);
    lv_obj_set_style_border_width(hub,0,0);

    info_label=text(right,"",16,56,416);
    lv_obj_set_style_text_line_space(info_label,-6,0);
    button(screen,16,234,146,48,manual_cb,&manual_label);
    button(screen,174,234,146,48,tare_cb,&tare_label);
    button(screen,16,294,304,46,sensor_cb,&sensor_label);
    button(screen,336,340,448,44,sync_cb,&sync_label);
    footer=text(screen,"",16,346,304);
    lv_obj_set_style_text_line_space(footer,-8,0);
    lv_obj_t *bar=lv_obj_create(screen);
    lv_obj_set_pos(bar,0,388); lv_obj_set_size(bar,800,36);
    lv_obj_set_style_bg_color(bar,lv_color_hex(0x262c32),0);
    lv_obj_set_style_border_width(bar,0,0); lv_obj_set_style_radius(bar,0,0);
    status_label=text(screen,"",16,390,768);
    lv_obj_set_style_text_line_space(status_label,-8,0);
    lv_obj_set_style_text_color(status_label,lv_color_hex(0x00ae42),0);
    lv_obj_set_style_bg_color(lv_obj_get_parent(sync_label),lv_color_hex(0x00ae42),0);
    screens_init();
    lv_timer_create(idle, 250, NULL);
    refresh();
    sb_display_unlock();
}
void ui_status(const char *key) {
    sb_display_lock();
    snprintf(status_key, sizeof(status_key), "%s", key);
    lv_label_set_text(status_label, translate(key));
    sb_display_unlock();
}
void ui_tag(const char *uid, const char *key) {
    sb_display_lock();
    snprintf(current_uid, sizeof(current_uid), "%s", uid ? uid : "");
    matched_id = 0;
    if(uid && uid[0]) screens_dashboard();
    snprintf(detail_key, sizeof(detail_key), "%s", key);
    refresh();
    sb_display_unlock();
}
void ui_details(const char *uid, const char *key) {
    sb_display_lock();
    if (!strcmp(current_uid, uid)) {
        snprintf(detail_key, sizeof(detail_key), "%s", key);
        render_weight();
    }
    sb_display_unlock();
}
void ui_spool(const char *uid, int id, const char *b, const char *m, float remaining, float core) {
    sb_display_lock();
    if (!strcmp(current_uid, uid)) {
        matched_id = id;
        snprintf(brand, sizeof(brand), "%s", b);
        snprintf(material, sizeof(material), "%s", m);
        core_grams = core;
        remaining_grams = remaining;
        render_weight();
    }
    sb_display_unlock();
}
void ui_weight(float g, bool stable, bool calibrated) {
    sb_display_lock();
    sensor_grams = g;
    sensor_valid = calibrated && isfinite(g) && g >= 0 && g <= 100000;
    sensor_stable = stable;
    render_weight();
    sb_display_unlock();
}
bool ui_take_tare(void) { return atomic_exchange(&tare_requested, false); }
bool ui_manual_active(void) { return atomic_load(&manual_flag); }
bool ui_take_manual(float *g) {
    sb_display_lock();
    bool ok = manual_pending;
    manual_pending = false;
    *g = manual_grams;
    sb_display_unlock();
    return ok;
}
bool ui_manual_value(float *g) {
    sb_display_lock();
    bool ok = manual_mode;
    *g = manual_grams;
    sb_display_unlock();
    return ok;
}
bool ui_take_sync(int *id, float *g) {
    sb_display_lock();
    bool ok = sync_pending;
    sync_pending = false;
    *id = sync_id;
    *g = sync_grams;
    sb_display_unlock();
    return ok;
}
void ui_display_settings(int b, int seconds) {
    sb_display_lock();
    if(!local_brightness) brightness = b < 0 ? 0 : b > 100 ? 100 : b;
    blank_seconds = seconds < 0 ? 0 : seconds > 86400 ? 86400 : seconds;
    sb_display_brightness(blanked ? 0 : brightness);
    sb_display_unlock();
}
void ui_wake(void) {
    sb_display_lock();
    lv_display_trigger_activity(NULL);
    blanked = false;
    sb_display_brightness(brightness);
    sb_display_unlock();
}
#ifdef SB_SIMULATOR
static lv_obj_t *find_text(lv_obj_t *root, const char *value) {
    if (lv_obj_check_type(root, &lv_label_class) && !strcmp(lv_label_get_text(root), value))
        return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); i++) {
        lv_obj_t *found = find_text(lv_obj_get_child(root, i), value);
        if (found)
            return found;
    }
    return NULL;
}
bool ui_test_click(const char *caption) {
    sb_display_lock();
    lv_obj_t *o = find_text(lv_screen_active(), caption);
    bool found = o != NULL;
    if (o)
        lv_obj_send_event(lv_obj_get_parent(o), LV_EVENT_CLICKED, NULL);
    sb_display_unlock();
    return found;
}
void ui_test_input(const char *value) {
    sb_display_lock();
    if (input)
        lv_textarea_set_text(input, value);
    sb_display_unlock();
}
bool ui_test_key(const char *value) {
    sb_display_lock();
    bool found = false;
    if (keyboard) {
        lv_obj_update_layout(keyboard);
        lv_area_t area;
        lv_obj_get_coords(keyboard, &area);
        if (area.x1 >= 0 && area.y1 >= 0 && area.x2 < 800 && area.y2 < 480) {
            const char *const *map = lv_buttonmatrix_get_map(keyboard);
            uint32_t button = 0;
            for (size_t i = 0; map[i] && map[i][0]; i++) {
                if (!strcmp(map[i], "\n"))
                    continue;
                if (!strcmp(map[i], value)) {
                    lv_buttonmatrix_set_selected_button(keyboard, button);
                    lv_obj_send_event(keyboard, LV_EVENT_VALUE_CHANGED, NULL);
                    found = true;
                    break;
                }
                button++;
            }
        }
    }
    sb_display_unlock();
    return found;
}
#endif

void ui_select_inventory(const char *uid, int id, const char *b, const char *m, float remaining, float core) {
    snprintf(current_uid,sizeof(current_uid),"%s",uid&&uid[0]?uid:"Inventory");
    matched_id=id; snprintf(brand,sizeof(brand),"%s",b); snprintf(material,sizeof(material),"%s",m);
    remaining_grams=remaining; core_grams=core; refresh();
}
void ui_local_brightness(int percent) {
    sb_display_lock();
    brightness=percent<10?10:percent>100?100:percent;
    local_brightness=true; blanked=false; sb_display_brightness(brightness);
    sb_display_unlock();
}
int ui_brightness_value(void) { return brightness; }
