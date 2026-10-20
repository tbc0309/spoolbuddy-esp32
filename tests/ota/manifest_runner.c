// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ota_manifest.h"
#include "settings.h"
#include "clock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv) {
    char input[32769];size_t n=fread(input,1,sizeof(input)-1,stdin);input[n]=0;
    if(argc>1 && !strcmp(argv[1],"settings")) {
        sb_settings_t config;char error[160];
        bool valid=sb_settings_parse(input,&config,error,sizeof(error));
        printf("{\"valid\":%s,\"chinese\":%s,\"override\":%s}",valid?"true":"false",
               valid && config.chinese?"true":"false",valid && config.language_override?"true":"false");
        return 0;
    }
    if(argc>1 && !strcmp(argv[1],"clock")) {
        cJSON *json=cJSON_Parse(input);const cJSON *date=cJSON_GetObjectItemCaseSensitive(json,"date");
        printf("{\"epoch\":%lld}",(long long)sb_http_date_epoch(cJSON_IsString(date)?date->valuestring:NULL));
        cJSON_Delete(json);return 0;
    }
    cJSON *json=cJSON_Parse(input);sb_ota_release r={0};
    int result=sb_ota_manifest_select(json,"esp32-p4-wifi6-touch-lcd-4.3/rev3",&r);
    cJSON *out=cJSON_CreateObject();cJSON_AddNumberToObject(out,"result",result);
    cJSON_AddBoolToObject(out,"newer",result==1 && sb_ota_newer(&r,argc>1?argv[1]:"1.2.5.7",argc>2?(uint32_t)strtoul(argv[2],NULL,10):1));
    cJSON_AddStringToObject(out,"sha256",r.sha256);
    if(result==1) {
        const cJSON *marker=cJSON_GetObjectItemCaseSensitive(json,"marker");
        if(cJSON_IsString(marker)) {
            sb_ota_identity_scanner scan;sb_ota_identity_init(&scan,&r);
            for(size_t i=0;i<=strlen(marker->valuestring);i++)
                sb_ota_identity_feed(&scan,marker->valuestring+i,1);
            cJSON_AddBoolToObject(out,"identity",scan.found);
        }
    }
    const cJSON *probe=cJSON_GetObjectItemCaseSensitive(json,"probe_url");
    if(cJSON_IsString(probe)) cJSON_AddBoolToObject(out,"github_auth",sb_ota_github(probe->valuestring));
    char *text=cJSON_PrintUnformatted(out);if(text) puts(text);
    cJSON_free(text);cJSON_Delete(out);cJSON_Delete(json);return 0;
}
