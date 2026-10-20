#pragma once
#include "lvgl.h"
#include <stdbool.h>
void sb_manual_weight_open(lv_obj_t *panel, const lv_font_t *font, bool chinese, void (*close)(void));
