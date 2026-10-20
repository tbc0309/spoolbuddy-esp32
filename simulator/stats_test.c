// Test the production ESP32 stats serializer with explicitly injected hardware snapshots.
#include "system_stats.h"
#include <stdio.h>
#include <stdlib.h>
int port_stats_test(void) {
    sb_system_snapshot_t s = {.arch = "riscv32",
                              .idf_version = "v6.1",
                              .cores = 2,
                              .uptime_us = 123456789,
                              .heap_total = 8388608,
                              .heap_free = 2097152,
                              .sd_available = true,
                              .sd_total = 17179869184ULL,
                              .sd_free = 12884901888ULL};
    cJSON *fixtures = cJSON_CreateObject();
    cJSON_AddBoolToObject(fixtures, "hardware_probed", false);
    cJSON_AddItemToObject(fixtures, "normal", sb_system_stats_build(&s));
    s.sd_available = false;
    cJSON_AddItemToObject(fixtures, "no_sd", sb_system_stats_build(&s));
    s.heap_total = 0;
    s.heap_free = 100;
    s.sd_available = true;
    s.sd_total = 0;
    cJSON_AddItemToObject(fixtures, "edge", sb_system_stats_build(&s));
    char *json = cJSON_PrintUnformatted(fixtures);
    FILE *f = fopen("esp32-stats-fixtures.json", "wb");
    int result = 1;
    if (f && json) {
        fputs(json, f);
        result = 0;
    }
    if (f)
        fclose(f);
    cJSON_free(json);
    cJSON_Delete(fixtures);
    return result;
}
