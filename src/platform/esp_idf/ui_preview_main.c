// SPDX-License-Identifier: AGPL-3.0-or-later
#include "memory_esp.h"
#include "ui_original.h"
#include "settings.h"
#include "board.h"
void app_main(void) {
    sb_memory_init();
    // Read the language only. Wi-Fi and backend tasks remain disabled in this target.
    char error[160];
    if (sb_board_sd_mount() == ESP_OK)
        sb_settings_read_file(sb_board_config_path(), error, sizeof(error));
    original_init(sb_settings.chinese);
}
