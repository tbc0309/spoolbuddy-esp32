#pragma once
#include "lvgl.h"
#include "cJSON.h"
/* Stable opt-in UI hooks. Implementations live only in patches/. */
void sb_manual_weight_open(lv_obj_t *,const lv_font_t *,bool,void (*close)(void));
void sb_confirm_open(lv_obj_t *,const char *,const char *,const cJSON *,bool,
    void (*close)(void),void (*sent)(const char *));
