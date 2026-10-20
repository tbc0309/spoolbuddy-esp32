#pragma once
#include <stddef.h>
typedef unsigned nvs_handle_t;
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL 1
#define NVS_READWRITE 1
int nvs_open_from_partition(const char *,const char *,int,nvs_handle_t *);
int nvs_get_str(nvs_handle_t,const char *,char *,size_t *);
int nvs_set_str(nvs_handle_t,const char *,const char *);
int nvs_commit(nvs_handle_t);
