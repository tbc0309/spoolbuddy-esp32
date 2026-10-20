#pragma once
#include "cJSON.h"
#include <stdbool.h>
#include <stddef.h>
void api_init(void);
const char *api_device_id(void);
cJSON *api_payload(void);
cJSON *api_post(const char *path, const cJSON *payload);
cJSON *api_get(const char *path);
int api_last_status(void);
cJSON *api_register(bool nfc, bool scale, int tare, double factor);
cJSON *api_heartbeat(bool nfc, bool scale);
void api_device_path(char *out, size_t size, const char *suffix);

cJSON *api_post_absolute(const char *path, const cJSON *body);

cJSON *api_request(const char *method, const char *path, const cJSON *body, int *status);
