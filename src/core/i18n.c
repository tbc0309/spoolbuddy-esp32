// SPDX-License-Identifier: AGPL-3.0-or-later
#include "i18n.h"
#include <stddef.h>
#include <string.h>
static const struct {const char *key,*en,*zh;} entries[]={
#include "../../assets/i18n_catalog.h"
};
const char *sb_i18n_text(const char *key,bool chinese) {
    size_t lo=0,hi=sizeof(entries)/sizeof(entries[0]);
    while(lo<hi) {
        size_t mid=lo+(hi-lo)/2;int compare=strcmp(key,entries[mid].key);
        if(!compare)return chinese?entries[mid].zh:entries[mid].en;
        if(compare<0)hi=mid;else lo=mid+1;
    }
    return key;
}
const char *sb_i18n_key(const char *text) {
    if(!text)return NULL;
    for(size_t i=0;i<sizeof(entries)/sizeof(entries[0]);i++)
        if(!strcmp(text,entries[i].key))return entries[i].key;
    for(size_t i=0;i<sizeof(entries)/sizeof(entries[0]);i++)
        if(!strcmp(text,entries[i].en) || !strcmp(text,entries[i].zh))return entries[i].key;
    return NULL;
}
