// SPDX-License-Identifier: AGPL-3.0-or-later
#include "memory_esp.h"
#include "settings_store_esp.h"
#include "config_record.h"
#include "cJSON.h"
#include "board.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
static nvs_handle_t store;
static bool ready;
static sb_config_record active;
static char *pending;
static int64_t pending_modified;
static int active_bank=-1;
static bool read_bank(int bank,sb_config_record *record) {
    const char *key=bank?"config1":"config0";size_t n=0;
    if(nvs_get_str(store,key,NULL,&n)!=ESP_OK || n<2 || n>16384)return false;
    char *text=sb_memory_alloc(n);if(!text)return false;
    bool ok=nvs_get_str(store,key,text,&n)==ESP_OK && sb_config_record_parse(text,record);
    free(text);return ok;
}
static char *read_card(char *error,size_t capacity,int64_t *modified) {
    struct stat st;const char *path=sb_board_config_path();
    if(stat(path,&st)!=0 || !S_ISREG(st.st_mode))return NULL;
    if(st.st_size<=0 || st.st_size>8192 || st.st_mtime<0) {
        snprintf(error,capacity,"Invalid TF configuration size/date");return NULL;
    }
    FILE *file=fopen(path,"rb");if(!file)return NULL;
    char *text=sb_memory_calloc(1,(size_t)st.st_size+1);
    bool ok=text && fread(text,1,st.st_size,file)==(size_t)st.st_size && !ferror(file) && fgetc(file)==EOF;
    fclose(file);
    if(ok && memchr(text,0,st.st_size))ok=false;
    if(!ok) {free(text);snprintf(error,capacity,"Cannot read TF configuration");return NULL;}
    *modified=st.st_mtime;return text;
}
bool sb_settings_store_load(bool sd_available,char *error,size_t capacity) {
    if(capacity)error[0]=0;
    esp_err_t err=nvs_flash_init_partition(sb_board_config_partition());
    if(err==ESP_OK)err=nvs_open_from_partition(sb_board_config_partition(),"settings",NVS_READWRITE,&store);
    ready=err==ESP_OK;
    if(ready) {
        sb_config_record banks[2]={0};bool ok[2]={read_bank(0,&banks[0]),read_bank(1,&banks[1])};
        active_bank=ok[1] && (!ok[0] || banks[1].generation>banks[0].generation)?1:ok[0]?0:-1;
        if(active_bank>=0) {active=banks[active_bank];sb_settings=active.settings;banks[active_bank].json=NULL;}
        sb_config_record_free(&banks[0]);sb_config_record_free(&banks[1]);
    } else ESP_LOGW("settings","Persistent configuration store unavailable: %s",esp_err_to_name(err));
    if(sd_available) {
        int64_t modified=0;char *text=read_card(error,capacity,&modified);
        if(text) {
            sb_settings_t candidate;
            if(sb_settings_parse(text,&candidate,error,capacity) && sb_config_candidate_newer(&active,text,modified)) {
                pending=text;pending_modified=modified;sb_settings=candidate;text=NULL;
                ESP_LOGI("settings","Trying TF configuration dated %lld; confirmation pending",(long long)modified);
            } else if(active.json)ESP_LOGI("settings","Keeping confirmed onboard configuration");
            free(text);
        }
    }
    if(!pending && active.json)ESP_LOGI("settings","Using confirmed onboard configuration, generation %lu",(unsigned long)active.generation);
    return pending || active.json;
}
bool sb_settings_store_confirm(void) {
    if(!pending)return true;
    if(!ready || active.generation==UINT32_MAX)return false;
    char *record=sb_config_record_encode(pending,active.generation+1,pending_modified);
    if(!record)return false;
    int bank=active_bank==0?1:0;
    esp_err_t err=nvs_set_str(store,bank?"config1":"config0",record);
    if(err==ESP_OK)err=nvs_commit(store);
    sb_config_record verified={0};
    bool ok=err==ESP_OK && read_bank(bank,&verified) && verified.generation==active.generation+1;
    cJSON_free(record);
    if(ok) {
        sb_config_record_free(&active);active=verified;active_bank=bank;
        free(pending);pending=NULL;
        ESP_LOGI("settings","Configuration confirmed and saved; previous bank retained as backup");
    } else {sb_config_record_free(&verified);ESP_LOGW("settings","Configuration save failed: %s",esp_err_to_name(err));}
    return ok;
}
bool sb_settings_store_rollback(void) {
    if(!pending || !active.json)return false;
    free(pending);pending=NULL;sb_settings=active.settings;
    ESP_LOGW("settings","Unconfirmed TF configuration timed out; restoring confirmed configuration");
    return true;
}
