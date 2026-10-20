// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "lvgl.h"
#include <stdbool.h>

typedef struct {
    lv_obj_t *mask;
    bool blank;
} sb_display_blanking;

/* Keep DSI scanning and frame-complete callbacks alive. Call with the LVGL lock.
 * The system layer covers dialogs too, and absorbs input until wake consumes it. */
static inline void sb_display_blanking_set(sb_display_blanking *state, lv_display_t *display,
                                           bool blank) {
    if (blank && !state->mask) {
        state->mask = lv_obj_create(lv_display_get_layer_sys(display));
        lv_obj_remove_style_all(state->mask);
        lv_obj_set_pos(state->mask, 0, 0);
        lv_obj_set_size(state->mask, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(state->mask, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(state->mask, LV_OPA_COVER, 0);
        lv_obj_remove_flag(state->mask, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(state->mask, LV_OBJ_FLAG_CLICKABLE);
    }
    if (state->mask) {
        if (blank) {
            lv_obj_remove_flag(state->mask, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(state->mask);
        } else {
            lv_obj_add_flag(state->mask, LV_OBJ_FLAG_HIDDEN);
        }
    }
    state->blank = blank;
}
