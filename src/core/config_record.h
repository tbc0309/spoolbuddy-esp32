#pragma once
#include "settings.h"
#include <stdint.h>
typedef struct {
    uint32_t generation;
    int64_t modified;
    char *json;
    sb_settings_t settings;
} sb_config_record;
void sb_config_record_free(sb_config_record *record);
bool sb_config_record_parse(const char *text,sb_config_record *record);
char *sb_config_record_encode(const char *json,uint32_t generation,int64_t modified);
bool sb_config_candidate_newer(const sb_config_record *active,const char *json,int64_t modified);
