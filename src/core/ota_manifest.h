// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "cJSON.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    char board[96], version[40], url[1536], sha256[65], file[96];
    uint32_t build, size;
} sb_ota_release;
/* 1 = valid release for this board, 0 = no published release, -1 = invalid. */
int sb_ota_manifest_select(const cJSON *json, const char *board, sb_ota_release *out);
bool sb_ota_newer(const sb_ota_release *release, const char *version, uint32_t build);
bool sb_ota_https(const char *url);
bool sb_ota_github(const char *url);
bool sb_ota_filename(const char *file);
typedef struct {
    unsigned char pattern[160], failure[160];
    size_t length, matched;
    bool found;
} sb_ota_identity_scanner;
bool sb_ota_identity_init(sb_ota_identity_scanner *scanner,const sb_ota_release *release);
void sb_ota_identity_feed(sb_ota_identity_scanner *scanner,const void *bytes,size_t size);
