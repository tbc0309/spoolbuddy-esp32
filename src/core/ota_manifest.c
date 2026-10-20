// SPDX-License-Identifier: AGPL-3.0-or-later
#include "ota_manifest.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
bool sb_ota_https(const char *url) {
    if (!url || strncmp(url,"https://",8) || !url[8]) return false;
    const char *end = strpbrk(url+8,"/?#");
    size_t n = end ? (size_t)(end-url-8) : strlen(url+8);
    return n && !strpbrk(url," \t\r\n\\") && !memchr(url+8,'@',n) && !strchr(url,'#');
}
bool sb_ota_github(const char *url) {
    const char *hosts[]={"https://api.github.com/","https://raw.githubusercontent.com/","https://github.com/"};
    for(size_t i=0;i<sizeof(hosts)/sizeof(hosts[0]);i++)
        if(url && !strncmp(url,hosts[i],strlen(hosts[i]))) return true;
    return false;
}
bool sb_ota_filename(const char *file) {
    if(!file || !*file || strlen(file)>=96 || file[0]=='.')return false;
    for(const unsigned char *p=(const unsigned char *)file;*p;p++)
        if(!isalnum(*p) && *p!='_' && *p!='-' && *p!='.')return false;
    return !strstr(file,"..");
}
static bool string(const cJSON *o,const char *key,char *to,size_t cap) {
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(o,key);
    if(!cJSON_IsString(v) || !v->valuestring[0] || strlen(v->valuestring)>=cap) return false;
    strcpy(to,v->valuestring); return true;
}
static bool integer(const cJSON *o,const char *key,uint32_t *to) {
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(o,key);
    if(!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble<0 ||
       v->valuedouble>UINT32_MAX || floor(v->valuedouble)!=v->valuedouble) return false;
    *to=(uint32_t)v->valuedouble; return true;
}
static bool version_parts(const char *s,unsigned long v[4]) {
    if(!s || !*s) return false;
    for(int i=0;i<4;i++) v[i]=0;
    int part=0; unsigned long value=0; bool digit=false;
    for(;*s;s++) {
        if(*s>='0' && *s<='9') { value=value*10+(unsigned)(*s-'0'); if(value>999999) return false; digit=true; }
        else if(*s=='.' && digit && part<3) {v[part++]=value;value=0;digit=false;}
        else return false;
    }
    if(!digit || part<2) return false;
    v[part]=value; return true;
}
bool sb_ota_newer(const sb_ota_release *r,const char *version,uint32_t build) {
    unsigned long a[4],b[4];
    if(!r || !version_parts(r->version,a) || !version_parts(version,b)) return false;
    for(int i=0;i<4;i++) if(a[i]!=b[i]) return a[i]>b[i];
    return r->build>build;
}
int sb_ota_manifest_select(const cJSON *json,const char *board,sb_ota_release *out) {
    uint32_t schema=0;
    const cJSON *releases=cJSON_GetObjectItemCaseSensitive(json,"releases");
    if(!integer(json,"schema_version",&schema) || schema!=1 || !cJSON_IsArray(releases) || !board || !out) return -1;
    bool found=false; sb_ota_release best={0}; const cJSON *item;
    cJSON_ArrayForEach(item,releases) {
        const cJSON *id=cJSON_GetObjectItemCaseSensitive(item,"board");
        if(!cJSON_IsString(id)) return -1;
        if(strcmp(id->valuestring,board)) continue;
        sb_ota_release r={0}; unsigned long parts[4];
        if(!string(item,"board",r.board,sizeof(r.board)) || !string(item,"version",r.version,sizeof(r.version)) ||
           !version_parts(r.version,parts) || !integer(item,"build",&r.build) ||
           !integer(item,"size",&r.size) || r.size<1024 || r.size>12*1024*1024 ||
           !string(item,"sha256",r.sha256,sizeof(r.sha256)) || strlen(r.sha256)!=64) return -1;
        const cJSON *url=cJSON_GetObjectItemCaseSensitive(item,"url");
        const cJSON *file=cJSON_GetObjectItemCaseSensitive(item,"file");
        if(url && (!string(item,"url",r.url,sizeof(r.url)) || !sb_ota_https(r.url)))return -1;
        if(file && (!string(item,"file",r.file,sizeof(r.file)) || !sb_ota_filename(r.file)))return -1;
        if(!url && !file)return -1;
        for(int i=0;i<64;i++) {if(!isxdigit((unsigned char)r.sha256[i])) return -1; r.sha256[i]=(char)tolower((unsigned char)r.sha256[i]);}
        if(!found || sb_ota_newer(&r,best.version,best.build)) best=r;
        found=true;
    }
    if(found) *out=best;
    return found?1:0;
}
bool sb_ota_identity_init(sb_ota_identity_scanner *s,const sb_ota_release *r) {
    if(!s || !r) return false;
    memset(s,0,sizeof(*s));
    int n=snprintf((char *)s->pattern,sizeof(s->pattern),"SB_OTA_ID:%s:%lu",r->board,(unsigned long)r->build);
    if(n<0 || n>=(int)sizeof(s->pattern)) return false;
    s->length=(size_t)n+1; /* Include NUL so build 1 cannot match build 10. */
    for(size_t i=1,j=0;i<s->length;i++) {
        while(j && s->pattern[i]!=s->pattern[j]) j=s->failure[j-1];
        if(s->pattern[i]==s->pattern[j]) j++;
        s->failure[i]=(unsigned char)j;
    }
    return true;
}
void sb_ota_identity_feed(sb_ota_identity_scanner *s,const void *bytes,size_t size) {
    if(!s || !s->length || s->found || !bytes) return;
    const unsigned char *p=bytes;
    for(size_t i=0;i<size;i++) {
        while(s->matched && p[i]!=s->pattern[s->matched]) s->matched=s->failure[s->matched-1];
        if(p[i]==s->pattern[s->matched]) s->matched++;
        if(s->matched==s->length) {s->found=true;return;}
    }
}
