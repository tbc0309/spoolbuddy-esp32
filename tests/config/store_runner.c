/* Real persistence controller; filesystem-backed NVS fixture across separate boots. */
#include "settings_store_esp.h"
#include "settings.h"
#include "nvs.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Host side of the platform allocation boundary; PSRAM is verified on P4. */
void *sb_memory_alloc(size_t size) {return malloc(size);}
void *sb_memory_calloc(size_t count,size_t size) {return calloc(count,size);}
static char *staged;static char staged_key[16];static int fail_commit;
const char *sb_board_config_path(void) {return "spoolbuddy.json";}
const char *sb_board_config_partition(void) {return "fixture";}
int nvs_flash_init_partition(const char *p) {(void)p;return 0;}
int nvs_open_from_partition(const char *p,const char *n,int mode,nvs_handle_t *out) {
    (void)p;(void)n;(void)mode;*out=1;return 0;
}
int nvs_get_str(nvs_handle_t h,const char *key,char *out,size_t *size) {
    (void)h;char path[64];snprintf(path,sizeof(path),"nvs_%s.json",key);
    FILE *f=fopen(path,"rb");if(!f)return 1;
    fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
    if(n<0) {fclose(f);return 1;}
    if(!out) {*size=(size_t)n+1;fclose(f);return 0;}
    if(*size<(size_t)n+1) {fclose(f);return 1;}
    int ok=fread(out,1,(size_t)n,f)==(size_t)n;out[n]=0;fclose(f);*size=(size_t)n+1;
    return ok?0:1;
}
int nvs_set_str(nvs_handle_t h,const char *key,const char *text) {
    (void)h;free(staged);staged=malloc(strlen(text)+1);if(!staged)return 1;
    strcpy(staged,text);snprintf(staged_key,sizeof(staged_key),"%s",key);return 0;
}
int nvs_commit(nvs_handle_t h) {
    (void)h;if(fail_commit)return 1;
    char path[64];snprintf(path,sizeof(path),"nvs_%s.json",staged_key);
    FILE *f=fopen(path,"wb");if(!f)return 1;
    size_t n=strlen(staged);int ok=fwrite(staged,1,n,f)==n;fclose(f);return ok?0:1;
}
int main(int argc,char **argv) {
    char error[160]={0};bool loaded=sb_settings_store_load(argc<3 || strcmp(argv[2],"no-sd"),error,sizeof(error));
    bool result=true;
    if(argc>1 && !strcmp(argv[1],"confirm"))result=sb_settings_store_confirm();
    if(argc>1 && !strcmp(argv[1],"fail-confirm")) {fail_commit=1;result=sb_settings_store_confirm();}
    if(argc>1 && !strcmp(argv[1],"rollback"))result=sb_settings_store_rollback();
    cJSON *out=cJSON_CreateObject();cJSON_AddBoolToObject(out,"loaded",loaded);
    cJSON_AddBoolToObject(out,"result",result);cJSON_AddStringToObject(out,"ssid",sb_settings.ssid);
    cJSON_AddBoolToObject(out,"language_override",sb_settings.language_override);
    cJSON_AddBoolToObject(out,"chinese",sb_settings.chinese);
    char *text=cJSON_PrintUnformatted(out);if(text)puts(text);
    cJSON_free(text);cJSON_Delete(out);free(staged);return 0;
}
