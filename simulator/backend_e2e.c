// SPDX-License-Identifier: AGPL-3.0-or-later
// Invoked only against the isolated upstream test application, never a user's backend.
#include "bridge.h"
#include "runtime.h"
#include "ui_original.h"
#include "ui_live.h"
#include "ui.h"
#include "system_stats.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
extern bool original_test_click(const char *caption);
extern bool original_test_input(const char *key, const char *value);
extern void original_test_detail(int id);
extern void original_test_writer(int id);
extern void original_test_close(void);
extern void original_test_colors(void);
static FILE *report;
static int failures, checks;
static void check(bool ok, const char *message) {
    fprintf(report, "%s %s\n", ok ? "PASS" : "FAIL", message);
    fflush(report);
    checks++;
    if (!ok)
        failures++;
}
static void click(const char *caption) { check(original_test_click(caption), caption); }
static double number(const cJSON *p, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(p, key);
    return cJSON_IsNumber(v) ? v->valuedouble : -1;
}
static void confirm(void) {
#ifdef SB_ENABLE_ACTION_CONFIRMATION
    click("Confirm");
#endif
    bridge_poll(sim_request, false);
}
int port_backend_e2e(void) {
    report = fopen("port-backend-e2e.txt", "w");
    if (!report)
        return 2;
    int status = 0;
    cJSON *guard = sim_request("GET", "/port-test-isolation", NULL, &status);
    const cJSON *isolated = cJSON_GetObjectItemCaseSensitive(guard, "isolated");
    bool safe = status == 200 && cJSON_IsTrue(isolated);
    cJSON_Delete(guard);
    check(safe, "isolated backend guard");
    if (!safe) {
        fclose(report);
        return 2;
    }
    cJSON *p = cJSON_Parse("{\"device_id\":\"p4-sim-windows\",\"hostname\":\"Native "
                           "E2E\",\"ip_address\":\"127.0.0.1\",\"firmware_version\":\"" SB_FIRMWARE_VERSION "\","
                           "\"has_nfc\":false,\"has_scale\":false,\"has_backlight\":true}");
    cJSON *r = sim_request("POST", "/spoolbuddy/devices/register", p, &status);
    check(status == 200 && r, "register with latest backend");
    cJSON_Delete(r);
    cJSON_Delete(p);
    sb_system_snapshot_t hardware = {.arch = "riscv32",
                                     .idf_version = "v6.1",
                                     .cores = 2,
                                     .heap_total = 8388608,
                                     .heap_free = 2097152,
                                     .uptime_us = 123000000};
    p = cJSON_CreateObject();
    cJSON_AddItemToObject(p, "system_stats", sb_system_stats_build(&hardware));
    r = sim_request("POST", "/spoolbuddy/devices/p4-sim-windows/heartbeat", p, &status);
    check(status == 200 && r, "latest backend accepts ESP32 hardware stats payload");
    cJSON_Delete(r);
    cJSON_Delete(p);
    original_language(false);
    bridge_online(true);
    bridge_poll(sim_request, true);
    const cJSON *registered_device = cJSON_GetArrayItem(bridge_model("devices"), 0);
    const cJSON *stats = cJSON_GetObjectItemCaseSensitive(registered_device, "system_stats");
    check(number(stats, "cpu_count") == 2 && number(stats, "system_uptime_s") == 123,
          "ESP32 chip and uptime stats survive real backend roundtrip");
    check(bridge_connected() && cJSON_GetArraySize(bridge_model("inventory")) == 0,
          "isolated inventory starts empty");
    original_page(2);
    original_subpage(1);
    original_form(0);
    check(original_test_input("brand", "P4 native E2E"), "new spool brand input");
    original_test_input("label_weight", "1000");
    original_test_colors();
    bridge_poll(sim_request, false);
    click("P4 native E2E · Forest");
    click("Create Spool");
    confirm();
    const cJSON *spool = cJSON_GetArrayItem(bridge_model("inventory"), 0);
    int id = (int)number(spool, "id");
    check(id > 0 && number(spool, "label_weight") == 1000,
          "LVGL creation persisted to real isolated database");
    const cJSON *rgba = cJSON_GetObjectItemCaseSensitive(spool, "rgba");
    check(cJSON_IsString(rgba) && !strcmp(rgba->valuestring, "00AE42FF"),
          "real backend color catalog applied to created spool");
    if (id <= 0) {
        fclose(report);
        return 1;
    }
    char profile_path[160];
    snprintf(profile_path, sizeof(profile_path), "%s/%d/k-profiles", bridge_inventory(), id);
    cJSON *profiles = cJSON_Parse(
        "[{\"extruder\":0,\"nozzle_diameter\":\"0.4\",\"k_value\":0.021,\"cali_idx\":4},{"
        "\"extruder\":1,\"nozzle_diameter\":\"0.6\",\"k_value\":0.035,\"cali_idx\":4}]");
    cJSON *profile;
    cJSON_ArrayForEach(profile, profiles)
        cJSON_AddNumberToObject(profile, "printer_id", bridge_printer_id());
    check(bridge_submit("PUT", profile_path, profiles),
          "queue multi-nozzle PA persistence to actual upstream route");
    cJSON_Delete(profiles);
    bridge_poll(sim_request, false);
    cJSON *stored = sim_request("GET", profile_path, NULL, &status);
    check(status == 200 && cJSON_GetArraySize(stored) == 2,
          "real backend stores both profiles with duplicate cali indices");
    cJSON_Delete(stored);
#ifdef SB_ENABLE_MANUAL_WEIGHT
    original_test_detail(id);
    bridge_poll(sim_request, false);
    click("Manual input");
    original_test_input("manual_weight", "1234.5");
    click("Apply");
#else
    ui_weight(1234.5, true, true); /* Inject a physical-scale fixture, no extra UI. */
#endif
    original_test_detail(id);
    bridge_poll(sim_request, false);
    click("Sync Weight");
    confirm();
    spool = cJSON_GetArrayItem(bridge_model("inventory"), 0);
    check(fabs(number(spool, "weight_used") - 15) < .01,
          "latest backend subtracts 250g core from upstream-rounded 1235g gross");
    original_test_writer(id);
    click("Write tag");
    confirm();
    const cJSON *device = cJSON_GetArrayItem(bridge_model("devices"), 0);
    const cJSON *pending = cJSON_GetObjectItemCaseSensitive(device, "pending_command");
    check(cJSON_IsString(pending) && !strcmp(pending->valuestring, "write_tag"),
          "NFC request queues real backend command without claiming a physical write");
    original_page(2);
    original_subpage(2);
    click("Cancel write");
    confirm();
    device = cJSON_GetArrayItem(bridge_model("devices"), 0);
    pending = cJSON_GetObjectItemCaseSensitive(device, "pending_command");
    check(!cJSON_IsString(pending), "cancel clears pending write in real backend");
    original_page(4);
    original_subpage(1);
    click("2m");
    bridge_poll(sim_request, false);
    device = cJSON_GetArrayItem(bridge_model("devices"), 0);
    check(number(device, "display_blank_timeout") == 120,
          "display timeout persisted by latest backend");
    original_subpage(2);
    click("Calibrate");
    ui_raw(100, true);
    click("Set Zero");
    original_test_input("calibration_weight", "500");
    ui_raw(1100, true);
    click("Calibrate");
    confirm();
    device = cJSON_GetArrayItem(bridge_model("devices"), 0);
    check(number(device, "tare_offset") == 100 &&
              fabs(number(device, "calibration_factor") - .5) < .001,
          "calibration factor computed by latest backend");
    char tag_path[160];
    snprintf(tag_path, sizeof(tag_path), "%s/%d/link-tag", bridge_inventory(), id);
    cJSON *tag = cJSON_Parse("{\"tag_uid\":\"AABBCCDD\",\"data_origin\":\"manual\"}");
    cJSON *linked = sim_request("PATCH", tag_path, tag, &status);
    cJSON_Delete(linked);
    cJSON_Delete(tag);
    bridge_poll(sim_request, true);
    original_test_writer(id);
    click("Untag Selected Spool");
    confirm();
    check(cJSON_GetArraySize(bridge_model("inventory")) == 1, "unlink preserves spool record");
    original_test_close();
    fprintf(report,
            "%d checks, %d failures. Real upstream routes, disposable database, no hardware.\n",
            checks, failures);
    fclose(report);
    return failures ? 1 : 0;
}
