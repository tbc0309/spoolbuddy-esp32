#pragma once
#include <stdbool.h>
#include <stddef.h>
bool sb_settings_store_load(bool sd_available,char *error,size_t capacity);
bool sb_settings_store_confirm(void);
bool sb_settings_store_rollback(void);
