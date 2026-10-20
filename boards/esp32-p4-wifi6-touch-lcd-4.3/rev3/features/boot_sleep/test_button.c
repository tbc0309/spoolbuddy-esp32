// SPDX-License-Identifier: AGPL-3.0-or-later
#include "button_state.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    sb_boot_button s = {.raw = true};
    assert(!sb_boot_button_poll(&s, true, 500));
    assert(!sb_boot_button_poll(&s, false, 510));
    assert(!sb_boot_button_poll(&s, false, 580));
    assert(s.armed);
    assert(!sb_boot_button_poll(&s, true, 600));
    assert(!sb_boot_button_poll(&s, false, 620)); /* bounce */
    assert(!sb_boot_button_poll(&s, false, 700));
    assert(!sb_boot_button_poll(&s, true, 800));
    assert(!sb_boot_button_poll(&s, true, 870));
    assert(!sb_boot_button_poll(&s, true, 9000)); /* hold never repeats */
    assert(!sb_boot_button_poll(&s, false, 9010));
    assert(sb_boot_button_poll(&s, false, 9080));
    assert(!sb_boot_button_poll(&s, false, 9200));
    assert(!sb_boot_button_poll(&s, true, 9300));
    assert(!sb_boot_button_poll(&s, true, 9370));
    assert(!sb_boot_button_poll(&s, false, 9400));
    assert(sb_boot_button_poll(&s, false, 9470));
    puts("PASS startup-held, debounce, hold, release, repeated press");
}
