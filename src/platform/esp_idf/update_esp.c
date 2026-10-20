// SPDX-License-Identifier: AGPL-3.0-or-later
#include "memory_esp.h"
#include "update.h"
#include "ota_manifest.h"
#include "settings.h"
#include "board.h"
#include "ui_platform.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "psa/crypto.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#ifndef SB_FIRMWARE_BUILD
#define SB_FIRMWARE_BUILD 1
#endif
#define SB_STRING_INNER(value) #value
#define SB_STRING(value) SB_STRING_INNER(value)
static const char firmware_identity[] __attribute__((used,retain))="SB_OTA_ID:" SB_BOARD_ID ":" SB_STRING(SB_FIRMWARE_BUILD);
const char *sb_update_identity(void) {return firmware_identity;}
static QueueHandle_t requests;
typedef struct {bool sd;} update_request;
static sb_update_state state;
typedef struct {char location[1536];} http_meta;
static void publish(sb_update_phase phase,const sb_ota_release *r,int percent,const char *message) {
    sb_display_lock();
    state.phase=phase; state.progress=percent;
    if(r) {
        state.artifact=true; state.build=r->build;
        snprintf(state.version,sizeof(state.version),"%s",r->version);
    }
    snprintf(state.message,sizeof(state.message),"%s",message?message:"");
    state.revision++;
    sb_display_unlock();
}
void sb_update_snapshot(sb_update_state *out) {sb_display_lock();*out=state;sb_display_unlock();}
static esp_err_t header(esp_http_client_event_t *e) {
    http_meta *m=e->user_data;
    if(e->event_id==HTTP_EVENT_ON_HEADER && e->header_key && !strcasecmp(e->header_key,"Location"))
        snprintf(m->location,sizeof(m->location),"%s",e->header_value?e->header_value:"");
    return ESP_OK;
}
/* Redirects are explicit so a private GitHub token is never sent to asset CDN hosts. */
static esp_http_client_handle_t open_http(const char *initial,bool image,http_meta *meta,
                                         char *error,size_t capacity) {
    char url[1536];
    if(!initial || strlen(initial)>=sizeof(url)) {snprintf(error,capacity,"Invalid OTA URL");return NULL;}
    strcpy(url,initial);
    for(int redirect=0;redirect<6;redirect++) {
        if(!sb_ota_https(url)) {snprintf(error,capacity,"OTA requires HTTPS");return NULL;}
        memset(meta,0,sizeof(*meta));
        esp_http_client_config_t cfg={.url=url,.timeout_ms=15000,.crt_bundle_attach=esp_crt_bundle_attach,
            .disable_auto_redirect=true,.event_handler=header,.user_data=meta,.buffer_size=4096,.buffer_size_tx=2048};
        esp_http_client_handle_t client=esp_http_client_init(&cfg);
        if(!client) {snprintf(error,capacity,"OTA HTTP allocation failed");return NULL;}
        esp_http_client_set_header(client,"User-Agent","SpoolBuddy-ESP32/" SB_FIRMWARE_VERSION);
        esp_http_client_set_header(client,"Accept",image?"application/octet-stream":"application/vnd.github.raw+json");
        if(sb_settings.ota_github_token[0] && sb_ota_github(url)) {
            char auth[280];snprintf(auth,sizeof(auth),"Bearer %s",sb_settings.ota_github_token);
            esp_http_client_set_header(client,"Authorization",auth);
        }
        esp_err_t err=esp_http_client_open(client,0);
        int64_t length=err==ESP_OK?esp_http_client_fetch_headers(client):-1;
        int code=esp_http_client_get_status_code(client);
        if(err==ESP_OK && length>=0 && code==200) return client;
        bool next=err==ESP_OK && (code==301||code==302||code==303||code==307||code==308) && meta->location[0];
        if(next) snprintf(url,sizeof(url),"%s",meta->location);
        else if(err!=ESP_OK) snprintf(error,capacity,"HTTPS request failed: %s",esp_err_to_name(err));
        else snprintf(error,capacity,"HTTP %d (private repository needs OTA credentials)",code);
        esp_http_client_close(client);esp_http_client_cleanup(client);
        if(!next) return NULL;
    }
    snprintf(error,capacity,"Too many OTA redirects");return NULL;
}
static void close_http(esp_http_client_handle_t client) {esp_http_client_close(client);esp_http_client_cleanup(client);}
static int check_release(sb_ota_release *release,char *error,size_t capacity) {
    publish(SB_UPDATE_CHECKING,NULL,0,"");
    const char *url=sb_settings.ota_manifest_url[0]?sb_settings.ota_manifest_url:CONFIG_SB_OTA_MANIFEST_URL;
    http_meta meta;
    esp_http_client_handle_t client=open_http(url,false,&meta,error,capacity);
    if(!client) return -1;
    char *body=sb_memory_calloc(1,16385);size_t used=0;bool valid=body!=NULL;
    while(valid) {
        if(used==16384) {valid=false;break;}
        int n=esp_http_client_read(client,body+used,16384-used);
        if(n<0) {valid=false;break;}
        if(n==0) {valid=esp_http_client_is_complete_data_received(client);break;}
        used+=(size_t)n;
    }
    close_http(client);
    cJSON *json=valid && used && !memchr(body,0,used)?cJSON_ParseWithOpts(body,NULL,true):NULL;
    int result=json?sb_ota_manifest_select(json,SB_BOARD_ID,release):-1;
    cJSON_Delete(json);free(body);
    if(result<0) snprintf(error,capacity,"Invalid or incompatible ota.json");
    return result;
}
static int check_sd_release(sb_ota_release *release,char *error,size_t capacity) {
    char path[160];snprintf(path,sizeof(path),"%s/ota.json",sb_board_sd_path());
    FILE *file=fopen(path,"rb");
    if(!file) {snprintf(error,capacity,"TF card ota.json not found");return -1;}
    char *body=sb_memory_calloc(1,16385);size_t used=body?fread(body,1,16384,file):0;
    bool valid=body && used && !ferror(file) && fgetc(file)==EOF && !memchr(body,0,used);
    fclose(file);
    cJSON *json=valid?cJSON_ParseWithOpts(body,NULL,true):NULL;
    int result=json?sb_ota_manifest_select(json,SB_BOARD_ID,release):-1;
    cJSON_Delete(json);free(body);
    if(result>0 && !sb_ota_filename(release->file))result=-1;
    if(result<0)snprintf(error,capacity,"Invalid TF card update manifest");
    return result;
}
static bool install(const sb_ota_release *release,bool sd,char *error,size_t capacity) {
    const esp_partition_t *partition=esp_ota_get_next_update_partition(NULL);
    if(!partition || release->size>partition->size) {snprintf(error,capacity,"No suitable OTA partition; USB bootstrap required");return false;}
    http_meta meta;esp_http_client_handle_t client=NULL;FILE *file=NULL;
    if(sd) {
        char path[256];snprintf(path,sizeof(path),"%s/%s",sb_board_sd_path(),release->file);
        file=fopen(path,"rb");
        if(!file) {snprintf(error,capacity,"TF card firmware file not found");return false;}
    } else {
        if(!release->url[0]) {snprintf(error,capacity,"No HTTPS firmware URL in ota.json");return false;}
        client=open_http(release->url,true,&meta,error,capacity);
        if(!client)return false;
        int64_t length=esp_http_client_get_content_length(client);
        if(length>0 && length!=release->size) {close_http(client);snprintf(error,capacity,"Firmware size mismatch");return false;}
    }
    uint8_t *buffer=sb_memory_alloc(4096);psa_hash_operation_t hash=PSA_HASH_OPERATION_INIT;
    esp_ota_handle_t handle=0;bool started=false,ok=false;
    esp_err_t err=ESP_FAIL;uint32_t total=0;int last=-1;
    sb_ota_identity_scanner identity;
    if(!sb_ota_identity_init(&identity,release)) goto done;
    if(!buffer || psa_crypto_init()!=PSA_SUCCESS || psa_hash_setup(&hash,PSA_ALG_SHA_256)!=PSA_SUCCESS) goto done;
    err=esp_ota_begin(partition,release->size,&handle);
    if(err!=ESP_OK) goto done;
    started=true;
    publish(SB_UPDATE_DOWNLOADING,release,0,"");
    for(;;) {
        int n=sd?(int)fread(buffer,1,4096,file):esp_http_client_read(client,(char *)buffer,4096);
        if(n<0) {err=ESP_ERR_TIMEOUT;goto done;}
        if(!n) {
            if(sd?ferror(file):!esp_http_client_is_complete_data_received(client)) {err=ESP_ERR_INVALID_RESPONSE;goto done;}
            break;
        }
        if((uint32_t)n>release->size-total) {err=ESP_ERR_INVALID_SIZE;goto done;}
        if(psa_hash_update(&hash,buffer,n)!=PSA_SUCCESS) {err=ESP_FAIL;goto done;}
        sb_ota_identity_feed(&identity,buffer,n);
        err=esp_ota_write(handle,buffer,n);if(err!=ESP_OK) goto done;
        total+=(uint32_t)n;int progress=(int)((uint64_t)total*100/release->size);
        if(progress!=last) {last=progress;publish(SB_UPDATE_DOWNLOADING,release,progress,"");}
    }
    if(total!=release->size) {err=ESP_ERR_INVALID_SIZE;goto done;}
    if(!identity.found) {err=ESP_ERR_INVALID_VERSION;goto done;}
    publish(SB_UPDATE_VERIFYING,release,100,"");
    uint8_t digest[32];size_t digest_size=0;char actual[65];
    if(psa_hash_finish(&hash,digest,sizeof(digest),&digest_size)!=PSA_SUCCESS || digest_size!=32) {err=ESP_FAIL;goto done;}
    for(int i=0;i<32;i++) snprintf(actual+i*2,3,"%02x",digest[i]);
    if(strcmp(actual,release->sha256)) {err=ESP_ERR_INVALID_CRC;goto done;}
    err=esp_ota_end(handle);started=false;
    if(err!=ESP_OK) goto done;
    esp_app_desc_t description;
    err=esp_ota_get_partition_description(partition,&description);
    if(err!=ESP_OK || strcmp(description.version,release->version) || strcmp(description.project_name,"spoolbuddy_port")) {
        err=ESP_ERR_INVALID_VERSION;goto done;
    }
    err=esp_ota_set_boot_partition(partition);ok=err==ESP_OK;
done:
    if(started) esp_ota_abort(handle);
    psa_hash_abort(&hash);free(buffer);
    if(file)fclose(file);
    if(client)close_http(client);
    if(!ok) snprintf(error,capacity,"OTA failed: %s",esp_err_to_name(err));
    return ok;
}
void sb_update_confirm_boot(void) {
    esp_ota_img_states_t status;
    if(esp_ota_get_state_partition(esp_ota_get_running_partition(),&status)==ESP_OK && status==ESP_OTA_IMG_PENDING_VERIFY)
        esp_ota_mark_app_valid_cancel_rollback();
}
static void worker(void *unused) {
    /* Allow the 120s configuration rollback to reconnect before rejecting an image. */
    (void)unused;int64_t deadline=esp_timer_get_time()+180000000;
    for(;;) {
        esp_ota_img_states_t boot;
        if(esp_timer_get_time()>deadline && esp_ota_get_state_partition(esp_ota_get_running_partition(),&boot)==ESP_OK && boot==ESP_OTA_IMG_PENDING_VERIFY)
            esp_ota_mark_app_invalid_rollback_and_reboot();
        update_request request={0};
        if(xQueueReceive(requests,&request,pdMS_TO_TICKS(1000))!=pdTRUE) continue;
        sb_ota_release release={0};char error[160]={0};
        int found=request.sd?check_sd_release(&release,error,sizeof(error)):check_release(&release,error,sizeof(error));
        if(found<0) {publish(SB_UPDATE_ERROR,NULL,0,error);continue;}
        sb_display_lock();state.artifact=found>0;sb_display_unlock();
        if(!found) {publish(SB_UPDATE_EMPTY,NULL,0,"");continue;}
        bool newer=sb_ota_newer(&release,SB_FIRMWARE_VERSION,SB_FIRMWARE_BUILD);
        if(!request.sd && !newer) {publish(SB_UPDATE_CURRENT,&release,0,"");continue;}
        sb_ota_release current={.build=SB_FIRMWARE_BUILD};
        snprintf(current.version,sizeof(current.version),"%s",SB_FIRMWARE_VERSION);
        if(sb_ota_newer(&current,release.version,release.build)) {publish(SB_UPDATE_ERROR,NULL,0,"OTA downgrade rejected");continue;}
        if(!install(&release,request.sd,error,sizeof(error))) {publish(SB_UPDATE_ERROR,NULL,0,error);continue;}
        publish(SB_UPDATE_REBOOTING,&release,100,"");
        vTaskDelay(pdMS_TO_TICKS(1500));esp_restart();
    }
}
void sb_update_init(void) {
    if(requests) return;
    ESP_LOGI("ota","%s",firmware_identity);
    requests=xQueueCreate(1,sizeof(update_request));
    if(!requests || xTaskCreate(worker,"ota",16384,NULL,3,NULL)!=pdPASS) {
        if(requests) vQueueDelete(requests);
        requests=NULL;publish(SB_UPDATE_ERROR,NULL,0,"OTA worker unavailable");
    }
}
static bool request_update(bool sd) {
    if(!requests) return false;
    sb_display_lock();
    bool busy=state.phase==SB_UPDATE_CHECKING || state.phase==SB_UPDATE_DOWNLOADING ||
              state.phase==SB_UPDATE_VERIFYING || state.phase==SB_UPDATE_REBOOTING;
    update_request request={.sd=sd};
    bool accepted=!busy && xQueueSend(requests,&request,0)==pdTRUE;
    if(accepted) {
        state.phase=SB_UPDATE_CHECKING;state.revision++;
        state.artifact=false;state.version[0]=0;state.message[0]=0;state.progress=0;
    }
    sb_display_unlock();return accepted;
}
bool sb_update_request(void) {return request_update(false);}
bool sb_update_request_sd(void) {return request_update(true);}
