#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint8_t uid[10];
    size_t uid_len;
    uint8_t sak;
} sb_tag_t;
esp_err_t pn_init(void);
esp_err_t pn_select(sb_tag_t *tag);
esp_err_t pn_read_tray(const sb_tag_t *tag, char tray[33]);
esp_err_t pn_write_ndef(const sb_tag_t *tag, const uint8_t *data, size_t len);
