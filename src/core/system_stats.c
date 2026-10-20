// SPDX-License-Identifier: AGPL-3.0-or-later
#include "system_stats.h"
#include <math.h>
#include <stdio.h>
static cJSON *memory(size_t total, size_t free_bytes) {
    if (free_bytes > total)
        free_bytes = total;
    cJSON *m = cJSON_CreateObject();
    cJSON_AddNumberToObject(m, "total_mb", round(total / 1048576.0 * 1000) / 1000);
    cJSON_AddNumberToObject(m, "available_mb", round(free_bytes / 1048576.0 * 1000) / 1000);
    cJSON_AddNumberToObject(m, "used_mb", round((total - free_bytes) / 1048576.0 * 1000) / 1000);
    cJSON_AddNumberToObject(m, "percent", total ? round(1000.0 * (total - free_bytes) / total) / 10 : 0);
    return m;
}
cJSON *sb_system_stats_build(const sb_system_snapshot_t *s) {
    if (!s)
        return NULL;
    cJSON *root = cJSON_CreateObject(), *os = cJSON_CreateObject();
    char platform[80];
    snprintf(platform, sizeof(platform), "ESP-IDF %s", s->idf_version ? s->idf_version : "unknown");
    cJSON_AddStringToObject(os, "os", platform);
    cJSON_AddStringToObject(os, "kernel", s->kernel ? s->kernel : "FreeRTOS");
    cJSON_AddStringToObject(os, "arch", s->arch ? s->arch : "unknown");
    cJSON_AddItemToObject(root, "os", os);
    cJSON_AddNumberToObject(root, "cpu_count", s->cores > 0 ? s->cores : 0);
    if (s->temperature_valid && isfinite(s->temperature_c))
        cJSON_AddNumberToObject(root, "cpu_temp_c", round((double)s->temperature_c * 10) / 10);
    cJSON_AddNumberToObject(root, "system_uptime_s", s->uptime_us / 1000000);
    cJSON_AddItemToObject(root, "memory", memory(s->heap_total, s->heap_free));
    /* Flash capacity is not a Linux filesystem; only mounted SD capacity is 'disk'. */
    if (s->sd_available && s->sd_total) {
        uint64_t free_bytes = s->sd_free > s->sd_total ? s->sd_total : s->sd_free;
        cJSON *disk = cJSON_CreateObject();
        cJSON_AddNumberToObject(disk, "total_gb", round(s->sd_total / 1073741824.0 * 100) / 100);
        cJSON_AddNumberToObject(disk, "free_gb", round(free_bytes / 1073741824.0 * 100) / 100);
        cJSON_AddNumberToObject(disk, "used_gb", round((s->sd_total - free_bytes) / 1073741824.0 * 100) / 100);
        cJSON_AddNumberToObject(disk, "percent", round(1000.0 * (s->sd_total - free_bytes) / s->sd_total) / 10);
        cJSON_AddItemToObject(root, "disk", disk);
    }
    return root;
}
