// SPDX-License-Identifier: AGPL-3.0-or-later
/* Private renderer for upstream Settings -> System. Included by ui_original.c. */
static void system_value(lv_obj_t *card, const char *label, const char *value, int y,
                         unsigned color) {
    text(card, label, 12, y, 132, MUTED, &small);
    lv_obj_t *v = text(card, value, 140, y, 222, color, &small);
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
}
static void system_usage(lv_obj_t *card, double percent, int x, int y, int width) {
    percent = fmin(100, fmax(0, percent));
    rect(card, x, y, width, 8, 0x3f3f46, 4);
    int used = (int)(width * percent / 100);
    if (used) rect(card, x, y, used, 8,
                   percent >= 90 ? 0xef4444 : percent >= 70 ? 0xf59e0b : 0x22c55e, 4);
}
static void system_settings(lv_obj_t *content) {
    const cJSON *stats = prop(own_device(), "system_stats");
    if (!sb_live.enabled || !cJSON_IsObject(stats)) {
        center_text(content, tr("spoolbuddy.settings.systemStatsWaiting"),
                    0, 56, 772, MUTED, &regular);
        return;
    }
    const cJSON *mem = prop(stats, "memory"), *disk = prop(stats, "disk"), *os = prop(stats, "os");
    char value[160];
    lv_obj_t *cpu = rect(content, 0, 0, 375, 116, CARD, 8);
    text(cpu, "CPU", 12, 12, 350, TEXT, &regular);
    snprintf(value, sizeof(value), "%.0f", number(stats, "cpu_count", 0));
    system_value(cpu, tr("spoolbuddy.settings.cores"), value, 38, TEXT);
    const cJSON *load = prop(stats, "load_avg");
    if (cJSON_IsArray(load) && cJSON_GetArraySize(load) >= 3)
        snprintf(value, sizeof(value), "%.2f / %.2f / %.2f", cJSON_GetArrayItem(load,0)->valuedouble,
                 cJSON_GetArrayItem(load,1)->valuedouble, cJSON_GetArrayItem(load,2)->valuedouble);
    else snprintf(value, sizeof(value), "—");
    system_value(cpu, tr("spoolbuddy.settings.loadAvg"), value, 62, TEXT);
    const cJSON *temp = prop(stats, "cpu_temp_c");
    if (cJSON_IsNumber(temp)) snprintf(value, sizeof(value), "%.1f°C", temp->valuedouble);
    else snprintf(value, sizeof(value), "—");
    system_value(cpu, tr("spoolbuddy.settings.temp"), value, 86,
                 number(stats,"cpu_temp_c",0) >= 80 ? 0xf87171 :
                 number(stats,"cpu_temp_c",0) >= 65 ? 0xfbbf24 : 0x4ade80);
    lv_obj_t *memory = rect(content, 383, 0, 376, 116, CARD, 8);
    text(memory, tr("spoolbuddy.settings.memory"), 12, 12, 350, TEXT, &regular);
    if (cJSON_IsObject(mem)) {
        system_usage(memory, number(mem,"percent",0),12,39,352);
        snprintf(value,sizeof(value),"%.1f / %.1f MB",number(mem,"used_mb",0),number(mem,"total_mb",0));
        system_value(memory,tr("spoolbuddy.settings.used"),value,59,TEXT);
        snprintf(value,sizeof(value),"%.1f MB",number(mem,"available_mb",0));
        system_value(memory,tr("spoolbuddy.settings.available"),value,83,TEXT);
    } else text(memory,"—",12,45,350,MUTED,&small);
    lv_obj_t *storage = rect(content,0,124,759,44,CARD,8);
    text(storage,tr("spoolbuddy.settings.disk"),12,14,72,TEXT,&regular);
    if (cJSON_IsObject(disk)) {
        system_usage(storage,number(disk,"percent",0),92,18,440);
        snprintf(value,sizeof(value),"%.2f / %.2f GB",number(disk,"used_gb",0),number(disk,"total_gb",0));
        lv_obj_t *v=text(storage,value,544,14,202,TEXT,&small);
        lv_obj_set_style_text_align(v,LV_TEXT_ALIGN_RIGHT,0);
    } else text(storage,"—",92,14,500,MUTED,&small);
    lv_obj_t *platform = rect(content,0,176,375,112,CARD,8);
    text(platform,tr("spoolbuddy.settings.osInfo"),12,12,350,TEXT,&regular);
    system_value(platform,tr("spoolbuddy.settings.distro"),str(os,"os"),38,TEXT);
    system_value(platform,tr("spoolbuddy.settings.kernel"),str(os,"kernel"),62,TEXT);
    system_value(platform,tr("spoolbuddy.settings.arch"),str(os,"arch"),86,TEXT);
    lv_obj_t *runtime=rect(content,383,176,376,112,CARD,8);
    text(runtime,tr("spoolbuddy.settings.runtime"),12,12,350,TEXT,&regular);
    int y=38;
    if (prop(os,"python")) { system_value(runtime,"Python",str(os,"python"),y,TEXT); y+=24; }
    if (prop(stats,"system_uptime_s")) {
        unsigned long seconds=(unsigned long)fmax(0,number(stats,"system_uptime_s",0));
        if (seconds>=86400) snprintf(value,sizeof(value),"%lud %luh %lum",seconds/86400,(seconds%86400)/3600,(seconds%3600)/60);
        else if(seconds>=3600) snprintf(value,sizeof(value),"%luh %lum",seconds/3600,(seconds%3600)/60);
        else snprintf(value,sizeof(value),"%lum",seconds/60);
        system_value(runtime,tr("spoolbuddy.settings.uptime"),value,y,TEXT);
    }
}
