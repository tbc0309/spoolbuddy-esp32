// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "lvgl.h"
#ifdef SB_ENABLE_BOARD_POWER_SAVE
void sb_power_init(void (*wake)(void));
void sb_power_settings(lv_obj_t *parent, bool chinese);
#else
static inline void sb_power_init(void (*wake)(void)) { (void)wake; }
static inline void sb_power_settings(lv_obj_t *parent, bool chinese) {
    (void)parent; (void)chinese;
}
#endif
