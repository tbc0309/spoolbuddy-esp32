// SPDX-License-Identifier: AGPL-3.0-or-later
#include "config_record.h"
#include "cJSON.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static char *canonical(const char *text) {
    cJSON *json=cJSON_ParseWithOpts(text,NULL,true);
    char *out=cJSON_IsObject(json)?cJSON_PrintUnformatted(json):NULL;
    cJSON_Delete(json);return out;
}
void sb_config_record_free(sb_config_record *r) {
    if(r) {cJSON_free(r->json);memset(r,0,sizeof(*r));}
}
bool sb_config_record_parse(const char *text,sb_config_record *r) {
    if(!text || !r)return false;
    cJSON *root=cJSON_ParseWithOpts(text,NULL,true);
    const cJSON *schema=cJSON_GetObjectItemCaseSensitive(root,"schema");
    const cJSON *gen=cJSON_GetObjectItemCaseSensitive(root,"generation");
    const cJSON *mtime=cJSON_GetObjectItemCaseSensitive(root,"modified");
    const cJSON *config=cJSON_GetObjectItemCaseSensitive(root,"configuration");
    bool ok=cJSON_IsNumber(schema) && schema->valuedouble==1 && cJSON_IsNumber(gen) &&
        isfinite(gen->valuedouble) && gen->valuedouble>=1 && gen->valuedouble<=UINT32_MAX &&
        floor(gen->valuedouble)==gen->valuedouble && cJSON_IsNumber(mtime) &&
        isfinite(mtime->valuedouble) && mtime->valuedouble>=0 && mtime->valuedouble<=4102444800.0 &&
        floor(mtime->valuedouble)==mtime->valuedouble && cJSON_IsObject(config);
    sb_config_record next={0};char error[160];
    if(ok) {
        next.json=cJSON_PrintUnformatted(config);
        ok=next.json && strlen(next.json)<=8192 && sb_settings_parse(next.json,&next.settings,error,sizeof(error));
    }
    if(ok) {
        next.generation=(uint32_t)gen->valuedouble;next.modified=(int64_t)mtime->valuedouble;
        next.settings.from_sd=false;*r=next;
    } else sb_config_record_free(&next);
    cJSON_Delete(root);return ok;
}
char *sb_config_record_encode(const char *text,uint32_t generation,int64_t modified) {
    sb_settings_t settings;char error[160];
    if(!text || !generation || modified<0 || modified>4102444800LL || strlen(text)>8192 ||
       !sb_settings_parse(text,&settings,error,sizeof(error)))return NULL;
    cJSON *config=cJSON_ParseWithOpts(text,NULL,true),*root=cJSON_CreateObject();
    if(!config || !root) {cJSON_Delete(config);cJSON_Delete(root);return NULL;}
    cJSON_AddNumberToObject(root,"schema",1);cJSON_AddNumberToObject(root,"generation",generation);
    cJSON_AddNumberToObject(root,"modified",(double)modified);cJSON_AddItemToObject(root,"configuration",config);
    char *out=cJSON_PrintUnformatted(root);cJSON_Delete(root);return out;
}
bool sb_config_candidate_newer(const sb_config_record *active,const char *text,int64_t modified) {
    if(!text || modified<0 || modified>4102444800LL)return false;
    if(!active || !active->json)return true;
    char *candidate=canonical(text);
    bool different=candidate && strcmp(candidate,active->json);
    cJSON_free(candidate);
    /* FAT has two-second timestamps: changed content at the same date is accepted. */
    return different && modified>=active->modified;
}
