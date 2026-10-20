// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    bool raw, stable, armed, pressed;
    uint32_t changed;
} sb_boot_button;
/* Debounced release action. A button held during startup is ignored until released. */
static inline bool sb_boot_button_poll(sb_boot_button *s, bool down, uint32_t now) {
    if (s->raw != down) { s->raw = down; s->changed = now; }
    if ((uint32_t)(now - s->changed) < 60) return false;
    if (!s->armed) {
        if (!down) { s->armed = true; s->stable = false; }
        return false;
    }
    if (s->stable == down) return false;
    s->stable = down;
    if (down) { s->pressed = true; return false; }
    bool action = s->pressed;
    s->pressed = false;
    return action;
}
