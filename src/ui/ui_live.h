#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    bool enabled, valid, stable, tare_pending;
    float sensor, core, remaining;
    int32_t raw;
    bool raw_valid;
    int spool_id, brightness, blank_seconds;
    char uid[33], tray_uuid[33], brand[80], material[80];
} sb_live_t;
extern sb_live_t sb_live;
void ui_manual_set(float grams);
void ui_manual_clear(void);
void original_live_changed(void);
void original_status(const char *message);
void ui_raw(int32_t raw, bool valid);
void ui_idle_tick(void);

void ui_tag_uuid(const char *uid, const char *uuid);
