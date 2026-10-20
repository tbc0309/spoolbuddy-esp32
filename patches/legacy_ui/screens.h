#pragma once
#include "cJSON.h"
#include "lvgl.h"
#include <stdbool.h>
typedef cJSON *(*sb_get_fn)(const char *path);
typedef cJSON *(*sb_post_fn)(const char *path, const cJSON *body);
/* GET/POST paths are absolute relative to /api/v1, unlike api_post(). */
void screens_init(void);
void screens_language(bool chinese);
void screens_network(sb_get_fn get, sb_post_fn post, const char *device, bool force);
void screens_online(bool online);
void screens_demo(void);
void screens_dashboard(void);
void screens_test_page(int page);
void ui_select_inventory(const char *uid, int id, const char *brand, const char *material,
                         float remaining, float core);
