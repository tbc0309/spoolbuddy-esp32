// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum { SB_UPDATE_IDLE, SB_UPDATE_CHECKING, SB_UPDATE_READY, SB_UPDATE_CURRENT,
    SB_UPDATE_EMPTY, SB_UPDATE_DOWNLOADING, SB_UPDATE_VERIFYING, SB_UPDATE_REBOOTING,
    SB_UPDATE_ERROR } sb_update_phase;
typedef struct {
    sb_update_phase phase;
    uint32_t revision, build;
    int progress;
    bool artifact;
    char version[40], message[160];
} sb_update_state;
void sb_update_init(void);
/* Check the manifest and automatically install only a newer release. */
bool sb_update_request(void);
bool sb_update_request_sd(void);
void sb_update_snapshot(sb_update_state *out);
/* Called after UI, Wi-Fi and backend registration pass on a new image. */
void sb_update_confirm_boot(void);
const char *sb_update_identity(void);
