// SPDX-License-Identifier: AGPL-3.0-or-later
#include <windows.h>
#include <stdio.h>
#include "realtime.h"
#include "runtime.h"
#include "ui_platform.h"
#include "lvgl.h"
int port_realtime_test(bool denied) {
    bridge_online(true);
    bridge_printer(3);
    ULONGLONG end = GetTickCount64() + 6500;
    bool got = false;
    while (GetTickCount64() < end) {
        sb_realtime_poll(true, sim_request);
        sb_display_lock();
        const cJSON *status = bridge_model("printer_status");
        got = got || cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(status, "connected"));
        sb_display_unlock();
        Sleep(20);
        lv_tick_inc(20);
    }
    sb_realtime_poll(false, sim_request);
    FILE *report = fopen("port-realtime-test.txt", "w");
    bool ok = denied ? !got : got;
    if (report) {
        fprintf(report, "%s native WebSocket %s; no inventory or hardware writes\n",
                ok ? "PASS" : "FAIL", denied ? "auth rejection" : "status event and reconnect");
        fclose(report);
    }
    return ok ? 0 : 1;
}
