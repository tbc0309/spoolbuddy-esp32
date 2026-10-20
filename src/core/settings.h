#pragma once
#include <stdbool.h>
#include <stddef.h>
typedef struct {
    char ssid[33];
    char password[64];
    char backend_url[256];
    char api_key[256];
    char ota_manifest_url[768];
    char ota_github_token[256];
    char timezone[64];
    bool chinese;
    bool language_override;
    bool from_sd;
} sb_settings_t;
extern sb_settings_t sb_settings;
bool sb_settings_parse(const char *json, sb_settings_t *out, char *error, size_t size);
bool sb_settings_read_file(const char *path, char *error, size_t size);
bool sb_parse_weight(const char *text, float *grams);
bool sb_language_is_chinese(const char *language);
