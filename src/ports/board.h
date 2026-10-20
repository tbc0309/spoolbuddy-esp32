#pragma once
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
/* Implemented by exactly one selected board directory. */
esp_err_t sb_board_sd_mount(void);
const char *sb_board_config_path(void);
const char *sb_board_config_partition(void);
const char *sb_board_sd_path(void);
esp_err_t sb_board_i2c_init(void);
i2c_master_bus_handle_t sb_board_i2c_bus(void);
const char *sb_board_name(void);
const char *sb_board_chip(void);
const char *sb_board_arch(void);
const char *sb_board_firmware_prefix(void);

const char *sb_board_device_prefix(void);

spi_host_device_t sb_board_nfc_spi_host(void);
/* Runtime input capability; ROM strapping is never modified. */
int sb_board_boot_gpio(void);
