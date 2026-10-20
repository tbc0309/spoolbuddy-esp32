#pragma once
#include "esp_err.h"
#include <stdint.h>
esp_err_t scale_init(void);
esp_err_t scale_read(int32_t *raw);
