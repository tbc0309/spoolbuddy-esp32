// SPDX-License-Identifier: AGPL-3.0-or-later
#include "update.h"
#include <stdio.h>
static sb_update_state state;
const char *sb_update_identity(void) {return "Windows simulator (no ESP32 image)";}
void sb_update_init(void) {}
bool sb_update_request(void) {
    state.phase=SB_UPDATE_ERROR; state.revision++;
    snprintf(state.message,sizeof(state.message),"%s","OTA runs on the ESP32 board");
    return true;
}
void sb_update_snapshot(sb_update_state *out) {*out=state;}
bool sb_update_request_sd(void) {
    state.phase=SB_UPDATE_ERROR;state.revision++;
    snprintf(state.message,sizeof(state.message),"Simulator cannot flash ESP32 firmware");return true;
}
void sb_update_confirm_boot(void) {}

/* Isolated native test injection; never compiled into ESP32 firmware. */
void sb_sim_update_state(const sb_update_state *fixture) {
    uint32_t next=state.revision+1;state=*fixture;state.revision=next;
}
