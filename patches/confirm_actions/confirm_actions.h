#pragma once
#include "lvgl.h"
#include "cJSON.h"
void sb_confirm_open(lv_obj_t *panel, const char *method,const char *path,const cJSON *body,
    bool chinese,void (*close)(void),void (*sent)(const char *path));
