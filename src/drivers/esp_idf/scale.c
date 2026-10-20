// SPDX-License-Identifier: AGPL-3.0-or-later
// NAU7802 sequencing follows Bambuddy's daemon/nau7802.py.
#include "scale.h"
#include "board.h"
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static i2c_master_dev_handle_t adc;
#define TRY(x)                                                                                     \
    do {                                                                                           \
        esp_err_t e = (x);                                                                         \
        if (e != ESP_OK)                                                                           \
            return e;                                                                              \
    } while (0)
static esp_err_t rd(uint8_t r, uint8_t *v, size_t n) {
    return i2c_master_transmit_receive(adc, &r, 1, v, n, 100);
}
static esp_err_t wr(uint8_t r, uint8_t v) {
    uint8_t b[] = {r, v};
    return i2c_master_transmit(adc, b, 2, 100);
}
static esp_err_t bits(uint8_t r, uint8_t mask, uint8_t v) {
    uint8_t b;
    TRY(rd(r, &b, 1));
    return wr(r, (b & ~mask) | (v & mask));
}
esp_err_t scale_read(int32_t *raw) {
    uint8_t v, b[3];
    TRY(rd(0, &v, 1));
    if (!(v & 0x20))
        return ESP_ERR_NOT_FOUND;
    TRY(rd(0x12, b, 3));
    uint32_t u = (uint32_t)b[0] << 16 | (uint32_t)b[1] << 8 | b[2];
    *raw = (u & 0x800000) ? (int32_t)(u | 0xff000000) : (int32_t)u;
    return ESP_OK;
}
static esp_err_t flush(int count) {
    int64_t deadline = esp_timer_get_time() + 2000000;
    while (count) {
        int32_t raw;
        esp_err_t e = scale_read(&raw);
        if (e == ESP_OK)
            count--;
        else if (e != ESP_ERR_NOT_FOUND)
            return e;
        if (esp_timer_get_time() > deadline)
            return ESP_ERR_TIMEOUT;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ESP_OK;
}
esp_err_t scale_init(void) {
    TRY(sb_board_i2c_init());
    i2c_device_config_t cfg = {.device_address = 0x2a, .scl_speed_hz = 100000};
    TRY(i2c_master_bus_add_device(sb_board_i2c_bus(), &cfg, &adc));
    TRY(wr(0, 1));
    vTaskDelay(pdMS_TO_TICKS(10));
    TRY(wr(0, 6));
    vTaskDelay(pdMS_TO_TICKS(600));
    uint8_t v;
    TRY(rd(0, &v, 1));
    if (!(v & 8))
        return ESP_ERR_TIMEOUT;
    TRY(rd(0x1f, &v, 1));
    if ((v & 15) != 15)
        return ESP_ERR_NOT_FOUND;
    TRY(bits(0, 0x80, 0x80));
    TRY(wr(1, 0x2f));
    TRY(wr(2, 0));
    TRY(bits(0x15, 0x30, 0x30));
    TRY(bits(0x1b, 0x40, 0));
    TRY(bits(0, 0x10, 0x10));
    TRY(flush(4));
    TRY(bits(2, 7, 4));
    int64_t end = esp_timer_get_time() + 1500000;
    do {
        TRY(rd(2, &v, 1));
        if (!(v & 4))
            break;
        vTaskDelay(pdMS_TO_TICKS(10));
    } while (esp_timer_get_time() < end);
    if (v & 12)
        return ESP_FAIL;
    return flush(2);
}
