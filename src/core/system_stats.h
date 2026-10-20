#pragma once
#include "cJSON.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
/* Hardware acquisition is platform-specific; normalization/JSON is shared and tested. */
typedef struct {
    const char *arch, *idf_version, *kernel;
    int cores;
    uint64_t uptime_us;
    size_t heap_total, heap_free;
    bool sd_available;
    bool temperature_valid;
    float temperature_c;
    uint64_t sd_total, sd_free;
} sb_system_snapshot_t;
cJSON *sb_system_stats_build(const sb_system_snapshot_t *snapshot);
