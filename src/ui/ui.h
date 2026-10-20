#pragma once
#include <stdbool.h>
void ui_init(void);
void ui_status(const char *text);
void ui_tag(const char *uid, const char *text);
void ui_details(const char *uid, const char *text);
void ui_weight(float grams, bool stable, bool calibrated);
bool ui_take_tare(void);
void ui_display_settings(int brightness, int blank_seconds);
void ui_wake(void);
void ui_language(bool chinese);
void ui_local_brightness(int percent);
int ui_brightness_value(void);
void ui_spool(const char *uid, int id, const char *brand, const char *material, float remaining,
              float core);
bool ui_manual_active(void);
bool ui_take_manual(float *grams);
bool ui_manual_value(float *grams);

#ifdef SB_SIMULATOR
bool ui_test_click(const char *caption);
void ui_test_input(const char *value);
bool ui_test_key(const char *value);
#endif

void ui_network(bool connected);
