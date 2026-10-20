// SPDX-License-Identifier: AGPL-3.0-or-later
#include "settings.h"
#include "cJSON.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
sb_settings_t sb_settings = {.backend_url = "http://192.168.1.100:8000", .timezone = "CST-8", .chinese = true};
bool sb_language_is_chinese(const char *language) {
    if (!language) return false;
    return (language[0]=='z' || language[0]=='Z') &&
           (language[1]=='h' || language[1]=='H') &&
           (!language[2] || language[2]=='-' || language[2]=='_');
}
static bool field(const cJSON *json, const char *key, char *out, size_t capacity) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!cJSON_IsString(v) || strlen(v->valuestring) >= capacity)
        return false;
    strcpy(out, v->valuestring);
    return true;
}
bool sb_settings_parse(const char *text, sb_settings_t *out, char *error, size_t size) {
    if (!text || !out)
        return false;
    const unsigned char *bytes = (const unsigned char *)text;
    if (strlen(text) >= 3 && bytes[0] == 0xef && bytes[1] == 0xbb && bytes[2] == 0xbf)
        text += 3;
    const char *end = NULL;
    cJSON *json = cJSON_ParseWithOpts(text, &end, true);
    sb_settings_t next = {.timezone = "CST-8", .chinese = true};
    cJSON *wifi = cJSON_GetObjectItemCaseSensitive(json, "wifi");
    cJSON *backend = cJSON_GetObjectItemCaseSensitive(json, "bambuddy");
    bool ok = cJSON_IsObject(json) && cJSON_IsObject(wifi) && cJSON_IsObject(backend) &&
              field(wifi, "ssid", next.ssid, sizeof(next.ssid)) && next.ssid[0] &&
              field(wifi, "password", next.password, sizeof(next.password)) &&
              field(backend, "url", next.backend_url, sizeof(next.backend_url)) &&
              field(backend, "api_key", next.api_key, sizeof(next.api_key));
    cJSON *language = cJSON_GetObjectItemCaseSensitive(json, "language");
    const cJSON *ota = cJSON_GetObjectItemCaseSensitive(json, "ota");
    if (ota) {
        if (!cJSON_IsObject(ota)) ok=false;
        if (cJSON_GetObjectItemCaseSensitive(ota,"manifest_url") &&
            !field(ota,"manifest_url",next.ota_manifest_url,sizeof(next.ota_manifest_url))) ok=false;
        if (next.ota_manifest_url[0] && strncmp(next.ota_manifest_url,"https://",8)) ok=false;
        if (cJSON_GetObjectItemCaseSensitive(ota,"github_token") &&
            !field(ota,"github_token",next.ota_github_token,sizeof(next.ota_github_token))) ok=false;
        if (strpbrk(next.ota_github_token,"\r\n\t ")) ok=false;
    }
    if (language) {
        if (!cJSON_IsString(language) || !language->valuestring[0] || strlen(language->valuestring)>31)
            ok = false;
        else {
            next.chinese = sb_language_is_chinese(language->valuestring);
            next.language_override = true;
        }
    }
    if (cJSON_GetObjectItemCaseSensitive(json,"timezone") &&
        (!field(json,"timezone",next.timezone,sizeof(next.timezone)) ||
         !next.timezone[0] || strpbrk(next.timezone,"\r\n\t "))) ok=false;
    size_t n = strlen(next.password);
    if (n && n < 8)
        ok = false;
    if (strncmp(next.backend_url, "http://", 7) && strncmp(next.backend_url, "https://", 8))
        ok = false;
    const char *host = strstr(next.backend_url, "://");
    if (!host || !host[3] || host[3] == '/' || strpbrk(next.backend_url, " \r\n\t?#") ||
        strstr(next.backend_url, "/api/v1"))
        ok = false;
    while (strlen(next.backend_url) > 8 && next.backend_url[strlen(next.backend_url) - 1] == '/')
        next.backend_url[strlen(next.backend_url) - 1] = 0;
    cJSON_Delete(json);
    if (!ok) {
        snprintf(error, size, "Invalid spoolbuddy.json: check Wi-Fi, server URL and language");
        return false;
    }
    next.from_sd = true;
    *out = next;
    if (size)
        error[0] = 0;
    return true;
}
bool sb_settings_read_file(const char *path, char *error, size_t size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        snprintf(error, size, "spoolbuddy.json not found");
        return false;
    }
    char text[8193];
    size_t n = fread(text, 1, 8192, file);
    int extra = fgetc(file);
    bool failed = ferror(file);
    fclose(file);
    if (failed || extra != EOF || !n) {
        snprintf(error, size, "Cannot read configuration (maximum 8 KiB)");
        return false;
    }
    text[n] = 0;
    if (memchr(text, 0, n)) {
        snprintf(error, size, "Configuration must be UTF-8 JSON");
        return false;
    }
    return sb_settings_parse(text, &sb_settings, error, size);
}
bool sb_parse_weight(const char *text, float *grams) {
    if (!text || !*text || !grams)
        return false;
    bool digit = false, point = false;
    for (const char *p = text; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            digit = true;
            continue;
        }
        if (*p == '.' && !point) {
            point = true;
            continue;
        }
        return false;
    }
    char *end;
    double value = strtod(text, &end);
    if (!digit || *end || !isfinite(value) || value < 0 || value > 100000)
        return false;
    *grams = (float)value;
    return true;
}
