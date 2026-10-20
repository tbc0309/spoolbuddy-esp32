#pragma once
#include "cJSON.h"
#include <stdbool.h>
typedef cJSON *(*sb_transport_fn)(const char *method, const char *path, const cJSON *body,
                                  int *status);
void bridge_init(const char *device_id);
void bridge_online(bool online);
void bridge_poll(sb_transport_fn transport, bool force);
bool bridge_submit(const char *method, const char *path, const cJSON *body);
const cJSON *bridge_model(const char *key); /* Caller holds the recursive display lock. */
const char *bridge_device(void);
const char *bridge_inventory(void);
void bridge_printer(int id);
int bridge_printer_id(void);
void bridge_refresh(void);
bool bridge_connected(void);
void original_model_changed(void);
void original_feedback(bool success, int status, const cJSON *body);

/* Queue an on-demand read; the JSON model is replaced on success. */
bool bridge_lookup(const char *key, const char *path);
/* Load form catalogs sequentially; never burst printer calibration requests. */
void bridge_load_catalogs(void);
bool bridge_catalogs_loading(void);

void bridge_reload_catalogs(void);
void bridge_event(const cJSON *event);
