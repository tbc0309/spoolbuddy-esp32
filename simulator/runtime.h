#pragma once
#include "cJSON.h"
#include <stdbool.h>
cJSON *sim_request(const char *method, const char *path, const cJSON *body, int *status);
bool sim_start(void);
void sim_stop(void);
