#pragma once
#include <stdbool.h>
void original_init(bool chinese);
void original_page(int page);
void original_subpage(int tab);
void original_language(bool chinese);
void original_online(bool online);

void original_form(int mode);

#include "cJSON.h"
void original_command_result(const char *method, const char *path, int code, const cJSON *reply);
