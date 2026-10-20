// SPDX-License-Identifier: AGPL-3.0-or-later
#include "memory_esp.h"
#include "cJSON.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "esp_log.h"
#include "lvgl.h"
#include "sdkconfig.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Prefer default-capability fallback rather than forcing internal/DMA RAM. */
void *sb_memory_alloc(size_t size) {
#ifdef CONFIG_SPIRAM
    return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                   MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT);
#else
    return malloc(size);
#endif
}
void *sb_memory_calloc(size_t count, size_t size) {
    if(count && size > SIZE_MAX / count) return NULL;
    size_t total=count*size;
    void *p=sb_memory_alloc(total);
    if(p) memset(p,0,total);
    return p;
}
void sb_memory_init(void) {
    cJSON_Hooks hooks={.malloc_fn=sb_memory_alloc,.free_fn=free};
    cJSON_InitHooks(&hooks);
    void *probe=sb_memory_alloc(64);
    ESP_LOGI("memory","PSRAM-first UI/JSON/buffers: external probe=%d, internal free=%u, PSRAM free=%u",
             probe && esp_ptr_external_ram(probe),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    free(probe);
}

/* LVGL's supported custom allocator: objects, styles, text and caches share
 * the same policy; display/DMA buffers remain under the board/BSP's control. */
#if LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM
void lv_mem_init(void) {}
void lv_mem_deinit(void) {}
lv_mem_pool_t lv_mem_add_pool(void *p,size_t n) {(void)p;(void)n;return NULL;}
void lv_mem_remove_pool(lv_mem_pool_t p) {(void)p;}
void *lv_malloc_core(size_t size) {return sb_memory_alloc(size);}
void lv_free_core(void *p) {free(p);}
void *lv_realloc_core(void *p,size_t size) {
#ifdef CONFIG_SPIRAM
    return heap_caps_realloc_prefer(p,size,2,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                   MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT);
#else
    return realloc(p,size);
#endif
}
void lv_mem_monitor_core(lv_mem_monitor_t *mon) {
    memset(mon,0,sizeof(*mon));
    unsigned caps=MALLOC_CAP_DEFAULT | MALLOC_CAP_8BIT;
    mon->total_size=heap_caps_get_total_size(caps);
    mon->free_size=heap_caps_get_free_size(caps);
    mon->free_biggest_size=heap_caps_get_largest_free_block(caps);
    if(mon->total_size) mon->used_pct=100*(mon->total_size-mon->free_size)/mon->total_size;
    if(mon->free_size) mon->frag_pct=100-100*mon->free_biggest_size/mon->free_size;
}
lv_result_t lv_mem_test_core(void) {
    return heap_caps_check_integrity_all(false)?LV_RESULT_OK:LV_RESULT_INVALID;
}
#endif
