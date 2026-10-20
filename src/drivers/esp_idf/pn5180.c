// SPDX-License-Identifier: AGPL-3.0-or-later
// Protocol port of Bambuddy spoolbuddy/daemon/pn5180.py (see THIRD_PARTY.md).
#include "pn5180.h"
#include "board.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "psa/crypto.h"
#include "sdkconfig.h"
#include <string.h>

static spi_device_handle_t spi;
static void delay_ms(int ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }
static esp_err_t busy_low(void) {
    int64_t end = esp_timer_get_time() + 1000000;
    while (gpio_get_level(CONFIG_SB_NFC_BUSY)) {
        if (esp_timer_get_time() > end)
            return ESP_ERR_TIMEOUT;
        delay_ms(1);
    }
    return ESP_OK;
}
static esp_err_t transfer(const uint8_t *tx, uint8_t *rx, size_t n) {
    esp_err_t err = busy_low();
    if (err != ESP_OK)
        return err;
    gpio_set_level(CONFIG_SB_NFC_CS, 0);
    esp_rom_delay_us(5);
    spi_transaction_t t = {.length = n * 8, .tx_buffer = tx, .rx_buffer = rx};
    err = spi_device_polling_transmit(spi, &t);
    gpio_set_level(CONFIG_SB_NFC_CS, 1);
    esp_rom_delay_us(100);
    return err;
}
static esp_err_t cmd(const uint8_t *p, size_t n) {
    esp_err_t err = transfer(p, NULL, n);
    if (err != ESP_OK)
        return err;
    // A short command may have completed before HIGH was sampled.
    int64_t end = esp_timer_get_time() + 1000;
    while (!gpio_get_level(CONFIG_SB_NFC_BUSY) && esp_timer_get_time() < end)
        esp_rom_delay_us(10);
    return busy_low();
}
static esp_err_t reg_op(uint8_t op, uint8_t reg, uint32_t v) {
    uint8_t b[] = {op, reg, v, v >> 8, v >> 16, v >> 24};
    return cmd(b, sizeof(b));
}
static esp_err_t reg_read(uint8_t reg, uint32_t *v) {
    uint8_t b[] = {4, reg}, rx[4], tx[4] = {255, 255, 255, 255};
    esp_err_t err = cmd(b, 2);
    if (err == ESP_OK)
        err = transfer(tx, rx, 4);
    if (err == ESP_OK)
        *v = (uint32_t)rx[0] | (uint32_t)rx[1] << 8 | (uint32_t)rx[2] << 16 | (uint32_t)rx[3] << 24;
    return err;
}
#define TRY(expr)                                                                                  \
    do {                                                                                           \
        esp_err_t e_ = (expr);                                                                     \
        if (e_ != ESP_OK)                                                                          \
            return e_;                                                                             \
    } while (0)
static esp_err_t crc(bool tx, bool rx) {
    TRY(reg_op(tx ? 1 : 2, 0x19, tx ? 1 : 0xfffffffe));
    return reg_op(rx ? 1 : 2, 0x12, rx ? 1 : 0xfffffffe);
}
static esp_err_t exchange(const uint8_t *data, size_t n, uint8_t bits, uint8_t *rx, size_t cap,
                          size_t *len) {
    uint32_t sys, status;
    TRY(reg_op(0, 3, 0xffffffff));
    TRY(reg_read(0, &sys));
    if (!(sys & 0x40))
        TRY(reg_op(0, 0, sys & 0xfffffff8));
    TRY(reg_op(0, 0, (sys & 0xfffffff8) | 3));
    uint8_t b[262] = {9, bits};
    if (n > sizeof(b) - 2)
        return ESP_ERR_INVALID_SIZE;
    memcpy(b + 2, data, n);
    TRY(cmd(b, n + 2));
    delay_ms(15);
    TRY(reg_read(0x13, &status));
    *len = status & 0x1ff;
    if (*len == 0)
        return ESP_ERR_NOT_FOUND;
    if (*len > cap || *len == 511)
        return ESP_ERR_INVALID_RESPONSE;
    uint8_t read[] = {0x0a, 0};
    TRY(cmd(read, 2));
    uint8_t dummy[260];
    memset(dummy, 255, sizeof(dummy));
    if (*len > sizeof(dummy))
        return ESP_ERR_INVALID_SIZE;
    return transfer(dummy, rx, *len);
}
esp_err_t pn_init(void) {
    gpio_config_t io = {.pin_bit_mask = (1ULL << CONFIG_SB_NFC_CS) | (1ULL << CONFIG_SB_NFC_RST),
                        .mode = GPIO_MODE_OUTPUT};
    TRY(gpio_config(&io));
    io.pin_bit_mask = 1ULL << CONFIG_SB_NFC_BUSY;
    io.mode = GPIO_MODE_INPUT;
    TRY(gpio_config(&io));
    gpio_set_level(CONFIG_SB_NFC_CS, 1);
    spi_bus_config_t bus = {.mosi_io_num = CONFIG_SB_NFC_MOSI,
                            .miso_io_num = CONFIG_SB_NFC_MISO,
                            .sclk_io_num = CONFIG_SB_NFC_SCK,
                            .quadwp_io_num = -1,
                            .quadhd_io_num = -1,
                            .max_transfer_sz = 264};
    TRY(spi_bus_initialize(sb_board_nfc_spi_host(), &bus, SPI_DMA_CH_AUTO));
    spi_device_interface_config_t dev = {
        .clock_speed_hz = 500000, .mode = 0, .spics_io_num = -1, .queue_size = 1};
    TRY(spi_bus_add_device(sb_board_nfc_spi_host(), &dev, &spi));
    gpio_set_level(CONFIG_SB_NFC_RST, 0);
    delay_ms(50);
    gpio_set_level(CONFIG_SB_NFC_RST, 1);
    delay_ms(100);
    uint8_t req[] = {7, 0x12, 2}, version[2], tx[] = {255, 255};
    TRY(cmd(req, 3));
    TRY(transfer(tx, version, 2));
    return ((version[0] == 255 && version[1] == 255) || (!version[0] && !version[1]))
               ? ESP_ERR_NOT_FOUND
               : ESP_OK;
}
esp_err_t pn_select(sb_tag_t *tag) {
    memset(tag, 0, sizeof(*tag));
    const uint8_t off[] = {0x17, 0}, rf[] = {0x11, 0, 0x80}, on[] = {0x16, 0};
    TRY(cmd(off, 2));
    delay_ms(5);
    TRY(reg_op(2, 0, 0xffffffbf));
    TRY(cmd(rf, 3));
    TRY(cmd(on, 2));
    delay_ms(10);
    TRY(crc(false, false));
    uint8_t rx[20], wake = 0x52;
    size_t n;
    TRY(exchange(&wake, 1, 7, rx, sizeof(rx), &n));
    if (n != 2)
        return ESP_ERR_INVALID_RESPONSE;
    const uint8_t levels[] = {0x93, 0x95, 0x97};
    for (size_t level = 0; level < 3; level++) {
        TRY(crc(false, false));
        uint8_t ac[] = {levels[level], 0x20};
        TRY(exchange(ac, 2, 0, rx, sizeof(rx), &n));
        if (n != 5 || (rx[0] ^ rx[1] ^ rx[2] ^ rx[3]) != rx[4])
            return ESP_ERR_INVALID_RESPONSE;
        uint8_t sel[7] = {levels[level], 0x70};
        memcpy(sel + 2, rx, 5);
        bool cascade = rx[0] == 0x88;
        size_t count = cascade ? 3 : 4;
        if (tag->uid_len + count > 10)
            return ESP_ERR_INVALID_SIZE;
        memcpy(tag->uid + tag->uid_len, rx + (cascade ? 1 : 0), count);
        tag->uid_len += count;
        TRY(crc(true, true));
        TRY(exchange(sel, 7, 0, rx, sizeof(rx), &n));
        tag->sak = rx[0];
        if (!(tag->sak & 4))
            return cascade ? ESP_ERR_INVALID_RESPONSE : ESP_OK;
        if (!cascade)
            return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_ERR_INVALID_RESPONSE;
}
static esp_err_t read_pages(uint8_t page, uint8_t data[16]) {
    TRY(crc(true, true));
    uint8_t req[] = {0x30, page};
    size_t n;
    TRY(exchange(req, 2, 0, data, 16, &n));
    return n == 16 ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}
static esp_err_t hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *data,
                             size_t data_len, uint8_t output[32]) {
    if (psa_crypto_init() != PSA_SUCCESS)
        return ESP_FAIL;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attributes, key_len * 8);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    mbedtls_svc_key_id_t id;
    psa_status_t status = psa_import_key(&attributes, key, key_len, &id);
    psa_reset_key_attributes(&attributes);
    if (status != PSA_SUCCESS)
        return ESP_FAIL;
    size_t written = 0;
    status =
        psa_mac_compute(id, PSA_ALG_HMAC(PSA_ALG_SHA_256), data, data_len, output, 32, &written);
    psa_destroy_key(id);
    return status == PSA_SUCCESS && written == 32 ? ESP_OK : ESP_FAIL;
}
esp_err_t pn_read_tray(const sb_tag_t *tag, char tray[33]) {
    tray[0] = 0;
    if ((tag->sak != 8 && tag->sak != 0x18) || tag->uid_len != 4)
        return ESP_ERR_NOT_SUPPORTED;
    const uint8_t master[] = {0x9a, 0x75, 0x9c, 0xf2, 0xc4, 0xf7, 0xca, 0xff,
                              0x22, 0x2c, 0xb9, 0x76, 0x9b, 0x41, 0xbc, 0x96};
    uint8_t prk[32], keybytes[32], context[] = {'R', 'F', 'I', 'D', '-', 'A', 0, 1};
    TRY(hmac_sha256(master, sizeof(master), tag->uid, 4, prk));
    TRY(hmac_sha256(prk, sizeof(prk), context, sizeof(context), keybytes));
    // Block 4 contains the 16-byte tray UUID; key for sector 1 is bytes 6..11.
    uint8_t auth[13] = {0x0c};
    memcpy(auth + 1, keybytes + 6, 6);
    auth[7] = 0x60;
    auth[8] = 4;
    memcpy(auth + 9, tag->uid, 4);
    TRY(cmd(auth, sizeof(auth)));
    uint8_t status, dummy = 255;
    TRY(transfer(&dummy, &status, 1));
    if (status)
        return ESP_ERR_INVALID_RESPONSE;
    uint8_t block[16];
    TRY(read_pages(4, block));
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < 16; i++) {
        tray[2 * i] = hex[block[i] >> 4];
        tray[2 * i + 1] = hex[block[i] & 15];
    }
    tray[32] = 0;
    return ESP_OK;
}
static esp_err_t write_page(uint8_t page, const uint8_t data[4]) {
    uint8_t req[6] = {0xa2, page}, rx[8];
    size_t n;
    memcpy(req + 2, data, 4);
    TRY(crc(true, false));
    esp_err_t err = exchange(req, sizeof(req), 0, rx, sizeof(rx), &n);
    // A 4-bit ACK may not be exposed; the caller verifies the stored bytes.
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND)
        return err;
    delay_ms(10);
    return ESP_OK;
}
esp_err_t pn_write_ndef(const sb_tag_t *tag, const uint8_t *data, size_t len) {
    // Current Bambuddy encoder prepends a CC copy. The real NTAG CC is page 3;
    // page 4 must start with TLV, otherwise other NFC readers cannot parse it.
    if (len >= 4 && data[0] == 0xe1) {
        data += 4;
        len -= 4;
    }
    if (tag->sak != 0 || len < 2 || len > 888 || data[0] != 0x03)
        return ESP_ERR_NOT_SUPPORTED;
    uint8_t cc[16];
    TRY(read_pages(3, cc));
    if (cc[0] != 0xe1 || (cc[1] >> 4) != 1 || cc[2] < 2 || (cc[3] & 15) != 0 ||
        len > (size_t)cc[2] * 8)
        return ESP_ERR_INVALID_SIZE;
    // Keep the TLV supplied by the backend, including NDEF length/terminator.
    // Preflight all page bounds before touching user memory.
    size_t padded = (len + 3) & ~(size_t)3;
    if (padded > (size_t)cc[2] * 8 || 4 + padded / 4 > 256)
        return ESP_ERR_INVALID_SIZE;
    TRY(reg_op(2, 0, 0xffffffbf));
    // Publish the NDEF length last, so an interrupted write leaves an empty TLV.
    const uint8_t empty[4] = {3, 0, 0, 0};
    TRY(write_page(4, empty));
    for (size_t page = 1; page <= padded / 4; page++) {
        size_t off = page == padded / 4 ? 0 : page * 4;
        uint8_t bytes[4] = {0};
        for (size_t j = 0; j < 4 && off + j < len; j++)
            bytes[j] = data[off + j];
        TRY(write_page(4 + off / 4, bytes));
    }
    for (size_t off = 0; off < padded; off += 16) {
        uint8_t actual[16];
        // READ always spans four pages; avoid wrapping beyond the user area.
        size_t start = off;
        if (start + 16 > (size_t)cc[2] * 8)
            start = (size_t)cc[2] * 8 - 16;
        TRY(read_pages(4 + start / 4, actual));
        size_t count = len - off < 16 ? len - off : 16;
        if (memcmp(actual + (off - start), data + off, count))
            return ESP_ERR_INVALID_RESPONSE;
    }
    return ESP_OK;
}
