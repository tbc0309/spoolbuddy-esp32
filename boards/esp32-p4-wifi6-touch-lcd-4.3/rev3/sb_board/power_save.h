// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <stdbool.h>
void sb_board_power_init(bool enabled, void (*wake)(void));
void sb_board_power_enable(bool enabled);
bool sb_board_power_enabled(void);
